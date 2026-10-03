//
// YaPB test host: unit/vision_{angles,sees,aim,nav}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for vision.cpp through a test-only hook (friend, no
// production behavior changes): angle math, frustum planes, visibility
// predicates, darkness logic and the aim-direction dispatcher. Randomness
// is pinned via exact modes and throttled timers, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct VisionHook {
  static void SetAimFlags (Bot &bot, AimFlags flags) {
    bot.aim_flags_ = flags;
  }
  static void SetLookAt (Bot &bot, const ystl::Vector &pos) {
    bot.look_at_ = pos;
  }
  static ystl::Vector LookAt (Bot &bot) {
    return bot.look_at_;
  }
  static void SetLookAtSafe (Bot &bot, const ystl::Vector &pos) {
    bot.look_at_safe_ = pos;
  }
  static void SetEntity (Bot &bot, const ystl::Vector &pos) {
    bot.entity_ = pos;
  }
  static void SetThrow (Bot &bot, const ystl::Vector &pos) {
    bot.throw_ = pos;
  }
  static void SetWantsFire (Bot &bot, bool value) {
    bot.wants_to_fire_ = value;
  }
  static void SetPath (Bot &bot, Path *path) {
    bot.path_ = path;
  }
  static void SetCurrent (Bot &bot, int index) {
    bot.current_node_index_ = index;
  }

  static float InFov (Bot &bot, const ystl::Vector &dest) {
    return bot.IsInFov (dest);
  }
  static bool InCone (Bot &bot, const ystl::Vector &origin) {
    return bot.IsInViewCone (origin);
  }
  static void BodyAngles (Bot &bot) {
    bot.UpdateBodyAngles ();
  }
  static bool SeesItem (Bot &bot, const ystl::Vector &dest, ystl::StringRef classname) {
    return bot.SeesItem (dest, classname);
  }
  static bool SeesC4 (Bot &bot, const ystl::Vector &dest) {
    return bot.SeesC4 (dest);
  }
  static bool SeesEnt (Bot &bot, const ystl::Vector &dest, bool from_body) {
    return bot.SeesEntity (dest, from_body);
  }
  static void Darkness (Bot &bot) {
    bot.CheckDarkness ();
  }
  static void LookAngles (Bot &bot) {
    bot.UpdateLookAngles ();
  }
  static void AimDirection (Bot &bot) {
    bot.SetAimDirection ();
  }
  static void SyncPath (Bot &bot, int src, int dst) {
    bot.path_enqueue_timer_.invalidate ();
    bot.FindPath (src, dst, FindPathType::Fast);
  }
  static void ChangeNode (Bot &bot, int index) {
    bot.ChangeNodeIndex (index);
  }
  static void SetMoveToGoal (Bot &bot, bool value) {
    bot.move_to_goal_ = value;
  }
  static void SetDestOrigin (Bot &bot, const ystl::Vector &pos) {
    bot.dest_origin_ = pos;
  }
  static void SetPathNode (Bot &bot, int index) {
    bot.path_ = &graph.paths_[static_cast<size_t> (index)];
  }
  static void SetPrevNode (Bot &bot, int index) {
    bot.previous_nodes_[0] = index;
  }
  static void SetPredict (Bot &bot, int index, int length) {
    bot.last_predict_index_ = index;
    bot.last_predict_length_ = length;
  }
  static AimFlags GetAimFlags (Bot &bot) {
    return bot.aim_flags_;
  }
  static const Frustum::Planes &FrustumPlanes (Bot &bot) {
    return bot.view_frustum_;
  }
};

namespace {

// four-node chain, dense numbering matches indices
void BuildVisionGraph () {
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
}

void BootVision (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildVisionGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
}

} // namespace

TEST_CASE ("unit/vision_angles") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootVision (engine, cs);

  bots.Addbot ("VisA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("VisA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);

  // absolute yaw gaps, wrap-around included
  CHECK (VisionHook::InFov (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)) == Approx (0.0).margin (0.01));
  CHECK (VisionHook::InFov (*bot, ystl::Vector (-100.0f, 0.0f, 0.0f)) == Approx (180.0).margin (0.01));
  CHECK (VisionHook::InFov (*bot, ystl::Vector (0.0f, 100.0f, 0.0f)) == Approx (90.0).margin (0.01));

  bot->pev->v_angle = ystl::Vector (0.0f, 350.0f, 0.0f);
  CHECK (VisionHook::InFov (*bot, ystl::Vector (98.48f, 17.36f, 0.0f)) == Approx (20.0).margin (0.5));

  // unnormalized view yaw: the difference must be wrapped, not the operands,
  // otherwise the raw gap exceeds 360 and the old 360-abs formula goes negative
  bot->pev->v_angle = ystl::Vector (0.0f, 400.0f, 0.0f);
  CHECK (VisionHook::InFov (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)) == Approx (40.0).margin (0.01));
  CHECK (VisionHook::InFov (*bot, ystl::Vector (-100.0f, 0.0f, 0.0f)) == Approx (140.0).margin (0.01));

  bot->pev->v_angle = ystl::Vector (0.0f, -400.0f, 0.0f);
  CHECK (VisionHook::InFov (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)) == Approx (40.0).margin (0.01));
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);

  // view cone follows fov through the support helper
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->fov = 90.0f;
  CHECK (VisionHook::InCone (*bot, ystl::Vector (500.0f, 0.0f, 0.0f)));
  CHECK (!VisionHook::InCone (*bot, ystl::Vector (-500.0f, 0.0f, 0.0f)));

  // body angles tilt the gun third-forward, and refresh the frustum
  bot->pev->v_angle = ystl::Vector (30.0f, 60.0f, 0.0f);
  VisionHook::BodyAngles (*bot);
  CHECK (bot->pev->angles.x == Approx (-10.0).margin (0.01));
  CHECK (bot->pev->angles.y == Approx (60.0).margin (0.01));

  edict_t *ahead = game.CreateFakeClient ("VisAhead");
  HOST_REQUIRE (!game.IsNullEntity (ahead));
  ahead->v.flags &= ~FL_FAKECLIENT;
  ahead->v.origin = ystl::Vector (500.0f, 150.0f, 0.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  VisionHook::BodyAngles (*bot);
  CHECK (frustum.Check (VisionHook::FrustumPlanes (*bot), ahead));

  ahead->v.origin = ystl::Vector (-500.0f, 0.0f, 0.0f);
  CHECK (!frustum.Check (VisionHook::FrustumPlanes (*bot), ahead));
  CHECK (!frustum.Check (VisionHook::FrustumPlanes (*bot), nullptr));
  ahead->free = 1;
  CHECK (!frustum.Check (VisionHook::FrustumPlanes (*bot), ahead));

  // plane math tolerates the bounding sphere, then cuts off
  Frustum::Plane probe {};
  probe.normal = ystl::Vector (1.0f, 0.0f, 0.0f);
  probe.result = 0.0f;
  CHECK (frustum.IsObjectInsidePlane (probe, ystl::Vector (5.0f, 0.0f, 0.0f), 72.0f, 16.0f));
  CHECK (!frustum.IsObjectInsidePlane (probe, ystl::Vector (-100.0f, 0.0f, 0.0f), 72.0f, 16.0f));

  // fov is horizontal: at fov 90 on 16:9 the vertical half-angle is
  // atan(1/aspect) ~29.36 deg (tan ~0.5625), the horizontal one is 45 deg
  // (tan 1). this pins the axis: a swapped height/width used to widen the
  // frustum vertically to 45 deg and horizontally to ~53.7 deg.
  Frustum::Planes probe_planes {};
  frustum.Calculate (probe_planes, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 0.0f), 90.0f);

  const auto top_side = static_cast<int> (Frustum::PlaneSide::Top);
  const auto right_side = static_cast<int> (Frustum::PlaneSide::Right);

  // vertical boundary at z/x = 0.5625: 0.5 inside, 0.7 outside
  CHECK (frustum.IsObjectInsidePlane (probe_planes[top_side], ystl::Vector (1000.0f, 0.0f, 500.0f), 0.0f, 0.0f));
  CHECK (!frustum.IsObjectInsidePlane (probe_planes[top_side], ystl::Vector (1000.0f, 0.0f, 700.0f), 0.0f, 0.0f));

  // horizontal boundary at |y|/x = 1.0: 0.6 inside, 1.4 outside
  CHECK (frustum.IsObjectInsidePlane (probe_planes[right_side], ystl::Vector (1000.0f, -600.0f, 0.0f), 0.0f, 0.0f));
  CHECK (!frustum.IsObjectInsidePlane (probe_planes[right_side], ystl::Vector (1000.0f, -1400.0f, 0.0f), 0.0f, 0.0f));

  // production kFov (75 horizontal): vertical boundary tan = tan(37.5)/aspect ~0.4317
  frustum.Calculate (probe_planes, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 0.0f), Frustum::kFov);

  CHECK (frustum.IsObjectInsidePlane (probe_planes[top_side], ystl::Vector (1000.0f, 0.0f, 350.0f), 0.0f, 0.0f));
  CHECK (!frustum.IsObjectInsidePlane (probe_planes[top_side], ystl::Vector (1000.0f, 0.0f, 500.0f), 0.0f, 0.0f));

  bots.Destroy ();
}

TEST_CASE ("unit/vision_sees") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootVision (engine, cs);

  bots.Addbot ("VisS", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("VisS");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);

  // clear world: everything resolves
  CHECK (VisionHook::SeesItem (*bot, ystl::Vector (400.0f, 0.0f, 0.0f), "func_button"));
  CHECK (VisionHook::SeesC4 (*bot, ystl::Vector (400.0f, 0.0f, 0.0f)));
  CHECK (VisionHook::SeesEnt (*bot, ystl::Vector (400.0f, 0.0f, 0.0f), false));
  CHECK (VisionHook::SeesEnt (*bot, ystl::Vector (400.0f, 0.0f, 0.0f), true));

  // blocked traces: classname hit, miss and tolerance band
  // (trace results cache for 0.1s, so each phase gets fresh time)
  edict_t *btn = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (btn != nullptr);
  const float btn_pos[3] = { 250.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (btn, btn_pos);

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([btn] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->flFraction = 0.5f;
    out->pHit = btn;
  });
  CHECK (VisionHook::SeesItem (*bot, ystl::Vector (400.0f, 0.0f, 0.0f), "func_button"));
  CHECK (!VisionHook::SeesItem (*bot, ystl::Vector (400.0f, 0.0f, 0.0f), "func_door"));
  CHECK (!VisionHook::SeesC4 (*bot, ystl::Vector (400.0f, 0.0f, 0.0f)));

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([btn] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->flFraction = 0.97f;
    out->pHit = btn;
  });
  CHECK (VisionHook::SeesItem (*bot, ystl::Vector (400.0f, 0.0f, 0.0f), "func_door"));

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->flFraction = 1.0f;
    out->fStartSolid = 1;
  });
  CHECK (!VisionHook::SeesItem (*bot, ystl::Vector (400.0f, 0.0f, 0.0f), "func_button"));
  CHECK (!VisionHook::SeesC4 (*bot, ystl::Vector (400.0f, 0.0f, 0.0f)));

  // darkness guards on timers, nodes and invalid light...
  CHECK (bot->pev->impulse == 0);
  VisionHook::Darkness (*bot); // spawn timer fresh
  CHECK (bot->pev->impulse == 0);

  engine.AdvanceTime (6.0f);
  VisionHook::SetCurrent (*bot, 0);
  VisionHook::SetPath (*bot, &graph.paths_[0]);
  VisionHook::Darkness (*bot); // invalid light level
  CHECK (bot->pev->impulse == 0);

  // ...then flashes on in the dark and off in the light
  // (each pass rearms the throttle timer first)
  graph.paths_[0].light = 0.0f;
  mp_flashlight.Set (1);
  VisionHook::Darkness (*bot);
  CHECK (bot->pev->impulse == 100);

  bot->pev->impulse = 0;
  bot->pev->effects |= EF_DIMLIGHT;
  graph.paths_[0].light = 100.0f;
  engine.AdvanceTime (5.0f);
  VisionHook::Darkness (*bot);
  CHECK (bot->pev->impulse == 100);
  bot->pev->effects &= ~EF_DIMLIGHT;

  // nightvision toggles through engine commands
  const int base = testhost::CsCalls (cs, "ClientCommand");
  bot->has_nvg_ = true;
  bot->pev->impulse = 0;
  graph.paths_[0].light = 0.0f;
  engine.AdvanceTime (5.0f);
  VisionHook::Darkness (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 1);

  graph.paths_[0].light = 100.0f;
  bot->uses_nvg_ = true;
  engine.AdvanceTime (5.0f);
  VisionHook::Darkness (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 2);
  bot->uses_nvg_ = false;
  bot->has_nvg_ = false;

  bots.Destroy ();
}

TEST_CASE ("unit/vision_aim") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootVision (engine, cs);

  bots.Addbot ("VisM", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("VisM");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);

  // empty sights default to straight ahead...
  VisionHook::SetLookAt (*bot, ystl::Vector {});
  engine.AdvanceTime (0.1f);
  VisionHook::LookAngles (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (600.0f, 0.0f, 28.0f));
  CHECK (bot->pev->v_angle.y == Approx (0.0).margin (0.01));

  // ...and swing toward off-axis targets without overshoot
  VisionHook::SetLookAt (*bot, ystl::Vector (100.0f + 433.0f, 250.0f, 28.0f));
  engine.AdvanceTime (0.1f);
  VisionHook::LookAngles (*bot);
  CHECK (bot->pev->v_angle.y > 0.0f);
  CHECK (bot->pev->v_angle.y <= 30.0f);
  CHECK (bot->pev->v_angle.z == 0.0f);

  // experts with fire orders snap to the solution
  bot->SetNewDifficulty (Difficulty::Expert);
  cv_whose_your_daddy.Set (1);
  VisionHook::SetAimFlags (*bot, AimFlags::Enemy);
  VisionHook::SetWantsFire (*bot, true);
  VisionHook::SetLookAt (*bot, ystl::Vector (600.0f, 0.0f, 28.0f));
  VisionHook::LookAngles (*bot);
  CHECK (bot->pev->v_angle.y == Approx (0.0).margin (0.5));
  cv_whose_your_daddy.Set (0);
  bot->SetNewDifficulty (Difficulty::Normal);

  // low tiers use the same aim mechanics, converging without overshoot
  bot->SetNewDifficulty (Difficulty::Noob);
  VisionHook::SetAimFlags (*bot, AimFlags::Enemy);
  VisionHook::SetLookAt (*bot, ystl::Vector (100.0f + 433.0f, 250.0f, 28.0f));
  engine.AdvanceTime (0.1f);
  VisionHook::LookAngles (*bot);
  CHECK (bot->pev->v_angle.y > 0.0f);
  CHECK (bot->pev->v_angle.y <= 30.0f);
  bot->SetNewDifficulty (Difficulty::Normal);

  // aim dispatcher: override, grenade arcs, entity and camp posts
  VisionHook::SetLookAt (*bot, ystl::Vector (1.0f, 2.0f, 3.0f));
  VisionHook::SetLookAtSafe (*bot, ystl::Vector (7.0f, 8.0f, 9.0f));
  VisionHook::SetAimFlags (*bot, AimFlags::Override);
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (7.0f, 8.0f, 9.0f));

  VisionHook::SetAimFlags (*bot, AimFlags::Grenade);
  VisionHook::SetThrow (*bot, ystl::Vector (300.0f, 0.0f, 0.0f));
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (300.0f, 0.0f, 0.0f));

  VisionHook::SetThrow (*bot, ystl::Vector (300.0f, 0.0f, 50.0f));
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (300.0f, 0.0f, 56.25f));

  VisionHook::SetAimFlags (*bot, AimFlags::Entity);
  VisionHook::SetEntity (*bot, ystl::Vector (11.0f, 12.0f, 13.0f));
  bot->pickup_type_ = Pickup::None;
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (11.0f, 12.0f, 13.0f));

  bot->pickup_type_ = Pickup::Hostage;
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (11.0f, 12.0f, 61.0f));

  bot->pickup_type_ = Pickup::Weapon;
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (11.0f, 12.0f, 85.0f));
  bot->pickup_type_ = Pickup::None;

  VisionHook::SetAimFlags (*bot, AimFlags::Camp);
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (7.0f, 8.0f, 9.0f));

  VisionHook::SetAimFlags (*bot, AimFlags::LastEnemy);
  bot->last_enemy_origin_ = ystl::Vector (21.0f, 22.0f, 23.0f);
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (21.0f, 22.0f, 23.0f));

  bots.Destroy ();
}

TEST_CASE ("unit/vision_nav") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootVision (engine, cs);

  // eight nodes for the loader-free flows below
  graph.Reset ();

  for (int i = 0; i < 8; ++i) {
    Path path {};
    path.origin = ystl::Vector (100.0f * i, 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = 100;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  for (int i = 0; i < 7; ++i) {
    link (i, i + 1);
    link (i + 1, i);
  }
  graph.PopulateNodes ();
  planner.Init ();
  practice.Load ();

  bots.Addbot ("VisN", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("VisN");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->flags |= FL_ONGROUND;
  bot->team_ = Team::Terrorist;

  edict_t *human = game.CreateFakeClient ("VisEnemy");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::CT;

  // walk the chain like the navigate scenario does
  VisionHook::SyncPath (*bot, 0, 3);
  VisionHook::ChangeNode (*bot, 0);
  VisionHook::SetMoveToGoal (*bot, true);
  VisionHook::SetDestOrigin (*bot, ystl::Vector (100.0f, 0.0f, 0.0f));
  VisionHook::SetPathNode (*bot, 0);
  graph.paths_[0].radius = 32;

  // dead predictions clear the flag straight away
  VisionHook::SetAimFlags (*bot, AimFlags::PredictPath);
  VisionHook::AimDirection (*bot);
  CHECK (!has_flag (VisionHook::GetAimFlags (*bot), AimFlags::PredictPath));

  // live predictions through visible nodes land exactly
  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());

  VisionHook::SetAimFlags (*bot, AimFlags::PredictPath);
  VisionHook::SetPredict (*bot, 3, 10);
  VisionHook::SetPrevNode (*bot, 0);
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (300.0f, 0.0f, 0.0f));

  // danger glances commit above the chord with hotspot math inside
  bots.SetEnemySpotted (true);
  bot->num_enemies_left_ = 1;
  practice.SetIndex (Team::Terrorist, 0, 0, 3);
  VisionHook::SetAimFlags (*bot, AimFlags::Nav);
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (300.0f, 0.0f, 17.0f));

  // then the walk lookahead takes the two-node point (past the glance)
  practice.SetIndex (Team::Terrorist, 0, 0, kInvalidNodeIndex);
  engine.AdvanceTime (12.0f);
  VisionHook::AimDirection (*bot);
  CHECK (VisionHook::LookAt (*bot) == ystl::Vector (200.0f, 0.0f, 28.0f));

  bots.Destroy ();
}

} // namespace bot
