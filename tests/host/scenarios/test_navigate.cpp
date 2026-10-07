//
// YaPB test host: unit/navigate_{path,goals,move,gaps,cover,bomb} + scenario/navigate_walk.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for navigate.cpp on synthetic graphs with a live Bot:
// pathfinding, node queries, goals, movement predicates, walking, and the
// leftover legs: rush timing, collision ignore/reset/execute, avoidance,
// terrain/fall checks, jump ballistics, async find/publish, occupancy and
// reachability predicates, path-origin jitter, alternative selection, lift
// handling and the navigation frame itself.
// Private Bot methods go through BotNavigateHook (friend, tests only).
//

#include <cstring>

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct NavigateHook {
  // pathfinding algorithms
  static ystl::RWrand &Rng (Bot &bot) {
    return bot.rg;
  }
  static bool ShortestPath (Bot &bot, int src, int dst, PathWalk &staging, NavigateData::PathMeta &meta) {
    bot.FindShortestPath (src, dst, staging, meta);
    return meta.has_path;
  }
  static void SyncPath (Bot &bot, int src, int dst, FindPathType type) {
    // test setup: bypass the rebuild throttle so consecutive calls take effect
    bot.path_enqueue_timer_.invalidate ();
    bot.FindPath (src, dst, type);
  }
  static int ChangeNode (Bot &bot, int index) {
    return bot.ChangeNodeIndex (index);
  }
  static int NearestNode (Bot &bot) {
    return bot.FindNearestNode ();
  }
  static int RandomNode (Bot &bot, PointType type) {
    return bot.FindRandomNode (type);
  }
  static int FarestNode (Bot &bot, const ystl::Vector &origin, float range) {
    return bot.FindFarestNode (origin, range);
  }
  static edict_t *LookupBtn (Bot &bot, ystl::StringRef target, bool blind) {
    return bot.LookupButton (target, blind);
  }
  static int AimingNode (Bot &bot, const ystl::Vector &to, int &length) {
    return bot.FindAimingNode (to, length);
  }
  static int CampDir (Bot &bot) {
    return bot.GetRandomCampDir ();
  }
  static int BestGoal (Bot &bot) {
    return bot.FindBestGoal ();
  }
  static int BombActionGoal (Bot &bot) {
    return bot.FindBestGoalWhenBombAction ();
  }
  static int HostageActionGoal (Bot &bot, ystl::SmallArray<int32_t> *defensive, ystl::SmallArray<int32_t> *offensive) {
    return bot.FindBestGoalWhenHostageAction (defensive, offensive);
  }
  static int GoalPost (Bot &bot, GoalTactic tactic, ystl::SmallArray<int32_t> *defensive, ystl::SmallArray<int32_t> *offensive) {
    return bot.FindGoalPost (tactic, defensive, offensive);
  }
  static void PostGoals (Bot &bot, const ystl::SmallArray<int32_t> &goals, int result[]) {
    bot.PostProcessGoals (goals, result);
  }
  static int BombNode (Bot &bot) {
    return bot.FindBombNode ();
  }
  static int DefendNode (Bot &bot, const ystl::Vector &origin) {
    return bot.FindDefendNode (origin);
  }
  static void MarkSite (Bot &bot, int node) {
    bot.MarkBombSiteVisited (node);
  }
  static int CoverNode (Bot &bot, float max_distance) {
    return bot.FindCoverNode (max_distance);
  }
  static bool TryTeleportToSealedBomb (Bot &bot) {
    return bot.TryTeleportToSealedBomb ();
  }

  // state setup
  static void SetHasC4 (Bot &bot, bool value) {
    bot.has_c4_ = value;
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
  static void SetDefendedBomb (Bot &bot, bool value) {
    bot.defended_bomb_ = value;
  }
  static bool DefendedBomb (Bot &bot) {
    return bot.defended_bomb_;
  }
  static int LoosedBomb (Bot &bot) {
    return bot.loosed_bomb_node_index_;
  }
  static void SetEnemyOrigin (Bot &bot, const ystl::Vector &pos) {
    bot.last_enemy_origin_ = pos;
  }
  static void SetAggression (Bot &bot, float aggression, float fear) {
    bot.agression_level_ = aggression;
    bot.fear_level_ = fear;
  }

  static void SetMoveSpeed (Bot &bot, float value) {
    bot.move_speed_ = value;
  }
  static void SetDestOrigin (Bot &bot, const ystl::Vector &pos) {
    bot.dest_origin_ = pos;
  }
  static void SetMoveAngles (Bot &bot, const ystl::Vector &angles) {
    bot.move_angles_ = angles;
  }
  static void SetTerrainStrafe (Bot &bot, float dir, float hold_time) {
    bot.terrain_strafe_dir_ = dir;
    bot.avoid_strafe_change_timer_.start (hold_time);
  }
  static void SetMovedDistance (Bot &bot, float value) {
    bot.moved_distance_ = value;
  }
  static void SetPrevSpeed (Bot &bot, float value) {
    bot.prev_speed_ = value;
  }
  static void SetStuckAccum (Bot &bot, float value) {
    bot.stuck_accumulated_time_ = value;
  }
  static void SetCurrentRaw (Bot &bot, int index) {
    bot.current_node_index_ = index;
  }
  static FindPathType PathType (Bot &bot) {
    return bot.path_type_;
  }
  static void SetWalk (Bot &bot, const ystl::Array<int32_t> &nodes) {
    bot.path_walk_.Clear ();
    for (const auto node : nodes) {
      bot.path_walk_.Add (node);
    }
  }
  static bool Advance (Bot &bot) {
    return bot.AdvanceMovement ();
  }
  static void MoveToGoal (Bot &bot) {
    bot.MoveToGoal ();
  }
  static void FindValid (Bot &bot) {
    bot.FindValidNode ();
  }
  static int NearestPlanted (Bot &bot) {
    return bot.GetNearestToPlantedBomb ();
  }
  static void TranslateInput (Bot &bot) {
    bot.TranslateInput ();
  }
  static void ResetMovement (Bot &bot) {
    bot.ResetMovement ();
  }
  static float ReachTime (Bot &bot) {
    return bot.GetEstimatedNodeReachTime ();
  }
  static float LadderDistance (Bot &bot) {
    return bot.ComputeLadderDesiredDistance ();
  }
  static bool ActiveGoal (Bot &bot) {
    return bot.HasActiveGoal ();
  }
  static float StrafeSpeed (Bot &bot) {
    return bot.strafe_speed_;
  }
  static edict_t *BlockedForward (Bot &bot, const ystl::Vector &normal) {
    return bot.IsBlockedForward (normal);
  }
  static bool JumpUp (Bot &bot, const ystl::Vector &normal) {
    return bot.CanJumpUp (normal);
  }
  static bool DuckUnder (Bot &bot, const ystl::Vector &normal) {
    return bot.CanDuckUnder (normal);
  }
  static bool StrafeLeft (Bot &bot, Trace::Result *tr) {
    return bot.CanStrafeLeft (tr);
  }
  static bool StrafeRight (Bot &bot, Trace::Result *tr) {
    return bot.CanStrafeRight (tr);
  }
  static bool BlockedLeft (Bot &bot) {
    return bot.IsBlockedLeft ();
  }
  static bool BlockedRight (Bot &bot) {
    return bot.IsBlockedRight ();
  }
  static bool WallLeft (Bot &bot, float distance) {
    return bot.CheckWallOnLeft (distance);
  }
  static bool WallRight (Bot &bot, float distance) {
    return bot.CheckWallOnRight (distance);
  }
  static bool WallBehind (Bot &bot, float distance) {
    return bot.CheckWallOnBehind (distance);
  }
  static bool DeadlyMove (Bot &bot, const ystl::Vector &to) {
    return bot.IsDeadlyMove (to);
  }
  static bool SafeToMove (Bot &bot, const ystl::Vector &to) {
    return bot.IsNotSafeToMove (to);
  }
  static bool WalkableAscent (Bot &bot, const ystl::Vector &src, const ystl::Vector &dst) {
    return bot.IsWalkableAscent (src, dst);
  }
  static bool StuckStatus (Bot &bot, const ystl::Vector &normal) {
    return bot.DetectStuckStatus (normal);
  }
  static void CollisionWeights (Bot &bot, const ystl::Vector &normal, Bot::CollisionWeights &weights) {
    bot.ComputeCollisionWeights (normal, weights);
  }
  static void StrafeSpeedTo (Bot &bot, const ystl::Vector &normal, float speed) {
    bot.SetStrafeSpeed (normal, speed);
  }
  static void StrafeSpeedRaw (Bot &bot, float speed) {
    bot.SetStrafeSpeedRaw (speed);
  }
  static void SetStrafe (Bot &bot, float speed) {
    bot.strafe_speed_ = speed;
  }
  static bool Stuck (Bot &bot) {
    return bot.IsStuckState ();
  }

  // state accessors
  static size_t WalkLength (Bot &bot) {
    return bot.path_walk_.Length ();
  }
  static int WalkAt (Bot &bot, size_t index) {
    return bot.path_walk_.At (index);
  }
  static int ChosenGoal (Bot &bot) {
    return bot.chosen_goal_index_;
  }
  static void SetChosenGoal (Bot &bot, int index) {
    bot.chosen_goal_index_ = index;
  }
  static bool Rush (Bot &bot) {
    return bot.ShouldRushEndgameTime ();
  }
  static void SetPersonality (Bot &bot, Personality personality) {
    bot.personality_ = personality;
  }
  static void SetHealthValue (Bot &bot, float value) {
    bot.health_value_ = value;
  }
  static void IgnoreColl (Bot &bot) {
    bot.IgnoreCollision ();
  }
  static void ResetColl (Bot &bot) {
    bot.ResetCollision ();
  }
  static CollisionState CollState (Bot &bot) {
    return bot.collision_state_;
  }
  static int CollIndex (Bot &bot) {
    return bot.coll_state_index_;
  }
  static void SetCollIndex (Bot &bot, int index) {
    bot.coll_state_index_ = index;
  }
  static void SetCollMove (Bot &bot, int index, CollisionState state) {
    bot.collide_moves_[index] = state;
  }
  static void ExecuteColl (Bot &bot) {
    bot.ExecuteCollisionResponse ();
  }
  static void Avoid (Bot &bot, const ystl::Vector &normal) {
    bot.DoPlayerAvoidance (normal);
  }
  static edict_t *Hindrance (Bot &bot) {
    return bot.hindrance_;
  }
  static void CheckTerr (Bot &bot, const ystl::Vector &normal) {
    bot.CheckTerrain (normal);
  }
  static void CheckF (Bot &bot) {
    bot.CheckFall ();
  }
  static bool Falling (Bot &bot) {
    return bot.is_fall_down_;
  }
  static ystl::Vector CalcJump (Bot &bot, const ystl::Vector &start, const ystl::Vector &stop) {
    return bot.CalcJumpVelocity (start, stop);
  }
  static void FindP (Bot &bot, int src, int dst) {
    bot.FindPath (src, dst, FindPathType::Fast);
  }
  static bool Occupied (Bot &bot, int index, bool need_zero_velocity) {
    return bot.IsOccupiedNode (index, need_zero_velocity);
  }
  static bool Reachable (Bot &bot, int index) {
    return bot.IsReachableNode (index);
  }
  static bool PrevLadder (Bot &bot) {
    return bot.IsPreviousLadder ();
  }
  static void SetPrevNode (Bot &bot, int slot, int value) {
    bot.previous_nodes_[slot] = value;
  }
  static void SetPathOr (Bot &bot) {
    bot.SetPathOrigin ();
  }
  static bool SelectNext (Bot &bot) {
    return bot.SelectBestNextNode ();
  }
  static bool UpdateNav (Bot &bot) {
    return bot.UpdateNavigation ();
  }
  static bool AimDirCan (Bot &bot) {
    return bot.can_set_aim_direction_;
  }
  static ystl::Vector DestOrigin (Bot &bot) {
    return bot.dest_origin_;
  }
  static bool LiftH (Bot &bot) {
    return bot.UpdateLiftHandling ();
  }
  static bool LiftS (Bot &bot) {
    return bot.UpdateLiftStates ();
  }
  static int GetLiftState (Bot &bot) {
    return ystl::to_underlying (bot.lift_state_);
  }
  static void SetLiftState (Bot &bot, LiftState state) {
    bot.lift_state_ = state;
  }
  static void SetLiftEntity (Bot &bot, edict_t *ent) {
    bot.lift_entity_ = ent;
  }
  static edict_t *LiftEntity (Bot &bot) {
    return bot.lift_entity_;
  }
  static void SetTravelFlags (Bot &bot, PathFlag flags) {
    bot.current_travel_flags_ = flags;
  }
  static ystl::Vector PathOrigin (Bot &bot) {
    return bot.path_origin_;
  }
};

namespace {

// chain 0 - 1 - 2 - 3 with an island, numbers match indices
void BuildNavGraph () {
  graph.Reset ();

  const ystl::Vector spots[5] = {
    ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::Vector (300.0f, 0.0f, 0.0f),
    ystl::Vector (2000.0f, 0.0f, 0.0f),
  };

  for (int i = 0; i < 5; ++i) {
    Path path {};
    path.origin = spots[i];
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  link (0, 1, 100);
  link (1, 0, 100);
  link (1, 2, 100);
  link (2, 1, 100);
  link (2, 3, 100);
  link (3, 2, 100);

  graph.PopulateNodes ();
  planner.Init ();
}

// unregistered bot: constructed directly (no BotManager), enough for the
// navigate algorithms (manager-owned lookups degrade to null gracefully)
ystl::UniquePtr<Bot> CreateNavBot (const char *name, const ystl::Vector &pos) {
  edict_t *ent = game.CreateFakeClient (name);

  if (game.IsNullEntity (ent)) {
    return {};
  }
  auto bot = ystl::make_unique<Bot> (ent, Difficulty::Normal, Personality::Normal, Team::CT, 0);

  bot->pev->origin = pos;
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  bot->pev->takedamage = DAMAGE_YES;
  bot->pev->solid = SOLID_BBOX;
  bot->pev->movetype = MOVETYPE_WALK;
  bot->pev->maxspeed = 270.0f;
  bot->is_alive_ = true;

  return bot;
}

} // namespace

TEST_CASE ("unit/navigate_path") {
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

  // graph first: the Bot ctor sizes its planner from it
  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("NavBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);

  // shortest path along the chain, exact sequence
  PathWalk staging {};
  staging.Init (64);
  NavigateData::PathMeta meta {};

  HOST_REQUIRE (NavigateHook::ShortestPath (*bot, 0, 3, staging, meta));
  HOST_REQUIRE (staging.Length () == 4);
  CHECK (staging.At (0) == 0);
  CHECK (staging.At (1) == 1);
  CHECK (staging.At (2) == 2);
  CHECK (staging.At (3) == 3);

  // island without links is unreachable, invalidation raised
  NavigateData::PathMeta island {};
  CHECK (!NavigateHook::ShortestPath (*bot, 0, 4, staging, island));
  CHECK (island.invalidate_prev_goal);
  CHECK (island.invalidate_goal_task);

  // sync variant publishes straight into the walk (single-threaded)
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  HOST_REQUIRE (NavigateHook::WalkLength (*bot) == 4);
  CHECK (NavigateHook::WalkAt (*bot, 0) == 0);
  CHECK (NavigateHook::WalkAt (*bot, 3) == 3);

  // node index bookkeeping (ctor leaves it invalid)
  CHECK (bot->GetCurrentNodeIndex () == kInvalidNodeIndex);
  CHECK (NavigateHook::ChangeNode (*bot, 0) == 0);
  CHECK (bot->GetCurrentNodeIndex () == 0);
  CHECK (NavigateHook::PathOrigin (*bot).x == 0.0f);
  CHECK (NavigateHook::ChangeNode (*bot, kInvalidNodeIndex) == kInvalidNodeIndex);
  CHECK (bot->GetCurrentNodeIndex () == 0); // invalid change keeps the old one

  // nearest node skips the current one: membership in the top candidates
  const int near = NavigateHook::NearestNode (*bot);
  CHECK (near == 1 || near == 2 || near == 3);

  // random/farest queries stay inside the graph (farest looks beyond range)
  CHECK (graph.Exists (NavigateHook::RandomNode (*bot, PointType::Count)));
  CHECK (NavigateHook::FarestNode (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), 1000.0f) == 4);

  // button lookup by target field, nearest wins
  edict_t *near_button = engine.SpawnEntity ("func_button");
  edict_t *far_button = engine.SpawnEntity ("func_button");
  edict_t *other_button = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (near_button != nullptr && far_button != nullptr && other_button != nullptr);

  const float near_pos[3] = { 50.0f, 0.0f, 0.0f };
  const float far_pos[3] = { 400.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (near_button, near_pos);
  engine.Funcs ().pfnSetOrigin (far_button, far_pos);
  near_button->v.target = string_t (engine.AllocString ("door1"));
  far_button->v.target = string_t (engine.AllocString ("door1"));
  other_button->v.target = string_t (engine.AllocString ("door2"));

  CHECK (NavigateHook::LookupBtn (*bot, "door1", true) == near_button);
  CHECK (NavigateHook::LookupBtn (*bot, "door1", false) == near_button);
  CHECK (NavigateHook::LookupBtn (*bot, "door9", true) == nullptr);
  CHECK (NavigateHook::LookupBtn (*bot, "", true) == nullptr);

  // far away the fallback resolves the single in-range candidate exactly
  bot->pev->origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  CHECK (NavigateHook::NearestNode (*bot) == 4);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  // no demolition map and no conf bomb model: no bomb goal
  CHECK (bot->GetNearestToPlantedBomb () == kInvalidNodeIndex);

  // aiming needs visibility data: empty vistable resolves to invalid
  int aim_length = 0;
  CHECK (NavigateHook::AimingNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f), aim_length) == kInvalidNodeIndex);

  // camp direction without visibility falls back to a valid node
  CHECK (graph.Exists (NavigateHook::CampDir (*bot)));

  // broken requests fail closed without crashing (errors are logged)
  NavigateHook::SyncPath (*bot, kInvalidNodeIndex, 3, FindPathType::Fast);
  NavigateHook::SyncPath (*bot, 0, 99, FindPathType::Fast);
  NavigateHook::SyncPath (*bot, 2, 2, FindPathType::Fast);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("scenario/navigate_walk") {
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

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("WalkBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->pev->flags |= FL_ONGROUND;

  // tasks: invalid goal first, then a real move task to node 3
  CHECK (!NavigateHook::ActiveGoal (*bot));
  bot->StartTask (TaskId::MoveTo, 50.0f, 3, 0.0f, false);

  // reach-time estimates from the previous/current spacing and speed
  // (ctor leaves current invalid, so step twice for a valid previous)
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 1);

  // freshly spawned counts as recently fired: base estimate regardless
  CHECK (NavigateHook::ReachTime (*bot) == 3.5f);

  // ladder leniency on an empty walk
  CHECK (NavigateHook::LadderDistance (*bot) == ystl::sqrf (15.0f));

  engine.AdvanceTime (0.3f); // outrun the just-fired window
  NavigateHook::SetMoveSpeed (*bot, 200.0f);
  CHECK (NavigateHook::ReachTime (*bot) == 3.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  CHECK (NavigateHook::ReachTime (*bot) == 3.5f);

  // sync path straightens the walk, then it drains node by node
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  HOST_REQUIRE (NavigateHook::WalkLength (*bot) == 4);
  CHECK (NavigateHook::ActiveGoal (*bot));

  // ladder leniency depends on the walk content
  ystl::Array<int32_t> flat_walk {};
  flat_walk.push (3);
  flat_walk.push (1);
  NavigateHook::SetWalk (*bot, flat_walk);
  CHECK (NavigateHook::LadderDistance (*bot) == ystl::sqrf (48.0f));

  float last_dist = bot->pev->origin.distance (ystl::Vector (300.0f, 0.0f, 0.0f));

  // rebuild the walk: the ladder probes above replaced it
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  NavigateHook::ChangeNode (*bot, 0);
  HOST_REQUIRE (NavigateHook::WalkLength (*bot) == 4);

  for (int step = 1; step <= 3; ++step) {
    HOST_REQUIRE (NavigateHook::Advance (*bot));
    CHECK (bot->GetCurrentNodeIndex () == step);

    // harness physics: the engine would drive velocity from buttons
    const ystl::Vector to = graph[bot->GetCurrentNodeIndex ()].origin - bot->pev->origin;
    const float dist = to.length ();

    if (dist > 1.0f) {
      bot->pev->velocity = to * (240.0f / dist);
    }
    engine.AdvanceTime (0.2f);

    const float now = bot->pev->origin.distance (ystl::Vector (300.0f, 0.0f, 0.0f));
    CHECK (now < last_dist);
    last_dist = now;

    NavigateHook::MoveToGoal (*bot);
  }
  CHECK (last_dist < 200.0f);

  // input translation turns speeds into buttons, reset clears everything
  NavigateHook::SetMoveSpeed (*bot, 200.0f);
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_FORWARD) != 0);

  NavigateHook::ResetMovement (*bot);
  CHECK (NavigateHook::StrafeSpeed (*bot) == 0.0f);
  CHECK (bot->pev->button == 0);

  // lost bots refind themselves, search state resets cleanly
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  CHECK (bot->FindNextBestNode ());
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);

  bot->ClearSearchNodes ();
  CHECK (NavigateHook::WalkLength (*bot) == 0);
  CHECK (NavigateHook::ChosenGoal (*bot) == kInvalidNodeIndex);

  // search type randomizes between fast and optimal
  bot->ResetPathSearchType ();
  const FindPathType path_type = NavigateHook::PathType (*bot);
  CHECK (path_type == FindPathType::Fast || path_type == FindPathType::Optimal);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_goals") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // demolition map for the planted-bomb branches below
  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  CHECK (game.MapIs (MapFlags::Demolition));

  // flagged chain: 0 plain, 1 T, 2 CT, 3 goal, 4 camp, 5 rescue
  graph.Reset ();

  const ystl::Vector spots[6] = {
    ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::Vector (300.0f, 0.0f, 0.0f),
    ystl::Vector (400.0f, 0.0f, 0.0f),
    ystl::Vector (500.0f, 0.0f, 0.0f),
  };
  const int32_t flag_of[6] = {
    0,
    ystl::to_underlying (NodeFlag::TerroristOnly),
    ystl::to_underlying (NodeFlag::CTOnly),
    ystl::to_underlying (NodeFlag::Goal),
    ystl::to_underlying (NodeFlag::Camp),
    ystl::to_underlying (NodeFlag::Rescue),
  };

  for (int i = 0; i < 6; ++i) {
    Path path {};
    path.origin = spots[i];
    path.number = i;
    path.flags = flag_of[i];
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  for (int i = 0; i < 5; ++i) {
    link (i, i + 1, 100);
    link (i + 1, i, 100);
  }
  graph.PopulateNodes ();
  planner.Init ();

  auto bot = CreateNavBot ("GoalBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);

  // post-processing fills distinct members of the input set
  ystl::SmallArray<int32_t> pool {};
  pool.push (1);
  pool.push (2);
  pool.push (3);
  pool.push (4);
  pool.push (5);

  int picked[4] = { kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex };
  NavigateHook::PostGoals (*bot, pool, picked);

  for (int i = 0; i < 4; ++i) {
    CHECK (picked[i] >= 1 && picked[i] <= 5);
    for (int j = i + 1; j < 4; ++j) {
      CHECK (picked[i] != picked[j]);
    }
  }

  // ... while the previous goal loses deterministically: exact match, no
  // area rule involved, so the output set is exactly {1, 2, 4, 5}
  bot->prev_goal_index_ = 3;

  int repicked[4] = { kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex };
  NavigateHook::PostGoals (*bot, pool, repicked);

  bool saw1 = false, saw2 = false, saw4 = false, saw5 = false, saw3 = false;
  for (int i = 0; i < 4; ++i) {
    HOST_REQUIRE (repicked[i] >= 1 && repicked[i] <= 5);
    saw1 = saw1 || repicked[i] == 1;
    saw2 = saw2 || repicked[i] == 2;
    saw3 = saw3 || repicked[i] == 3;
    saw4 = saw4 || repicked[i] == 4;
    saw5 = saw5 || repicked[i] == 5;

    for (int j = i + 1; j < 4; ++j) {
      CHECK (repicked[i] != repicked[j]);
    }
  }
  CHECK (saw1 && saw2 && saw4 && saw5 && !saw3);
  bot->prev_goal_index_ = kInvalidNodeIndex;

  // tactic posts resolve into their own pools
  ystl::SmallArray<int32_t> defensive {};
  defensive.push (2);
  ystl::SmallArray<int32_t> offensive {};
  offensive.push (1);

  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::Defensive, &defensive, &offensive) == 2);
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::Offensive, &defensive, &offensive) == 1);
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::Camp, &defensive, &offensive) == 4);
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive) == 3);
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::RescueHostage, &defensive, &offensive) == 5);
  CHECK (NavigateHook::ChosenGoal (*bot) == 5);

  ystl::SmallArray<int32_t> empty {};
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Defensive, &empty, &empty)));

  // vip and c4 carriers take the closest goal
  bot->is_vip_ = true;
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive) == 3);
  bot->is_vip_ = false;

  NavigateHook::SetHasC4 (*bot, true);
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive) == 3);
  NavigateHook::SetHasC4 (*bot, false);

  // best goal stays valid across teams and personalities
  for (int i = 0; i < 3; ++i) {
    CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  }
  bot->team_ = Team::Terrorist;
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  bot->team_ = Team::CT;

  game.AddGameFlag (GameFlags::ZombieMod);
  NavigateHook::SetCreature (*bot, true);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  game.ClearGameFlag (GameFlags::ZombieMod);
  NavigateHook::SetCreature (*bot, false);

  // loose bomb: dropped c4 resolves to its nearest node
  edict_t *loose = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (loose != nullptr);
  loose->v.model = string_t (engine.AllocString ("models/p_backpack.mdl"));

  const float loose_pos[3] = { 250.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (loose, loose_pos);

  CHECK (NavigateHook::BombActionGoal (*bot) == 2);
  CHECK (NavigateHook::LoosedBomb (*bot) == 2);

  // planted bomb: defend flow runs once, then goes quiet
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (250.0f, 0.0f, 0.0f));

  NavigateHook::SetDefendedBomb (*bot, false);
  const int defend_goal = NavigateHook::BombActionGoal (*bot);
  CHECK (graph.Exists (defend_goal));
  CHECK (NavigateHook::DefendedBomb (*bot));
  CHECK (NavigateHook::BombActionGoal (*bot) == kInvalidNodeIndex);

  // audible bomb shortcuts to the node nearest the sound (node 2 at 200)
  CHECK (NavigateHook::BombNode (*bot) == 2);

  // hostage carrier without visible hostages takes the rescue point
  bot->has_hostage_ = true;
  CHECK (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive) == 5);
  bot->has_hostage_ = false;

  // cover without visibility data treats everything as hidden: real selection
  NavigateHook::SetEnemyOrigin (*bot, ystl::Vector (500.0f, 0.0f, 0.0f));
  CHECK (graph.Exists (NavigateHook::CoverNode (*bot, 1000.0f)));

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_move") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // breakable + door seeds for the exemption branches below
  edict_t *seeded = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (seeded != nullptr);
  seeded->v.health = 50.0f;
  seeded->v.takedamage = DAMAGE_YES;
  seeded->v.movetype = MOVETYPE_PUSH;

  edict_t *door = engine.SpawnEntity ("func_door");
  HOST_REQUIRE (door != nullptr);

  edict_t *hostage = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hostage != nullptr);

  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  CHECK (game.MapIs (MapFlags::HasDoors));

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("MoveBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->pev->angles = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->flags |= FL_ONGROUND;
  bot->pev->maxspeed = 270.0f;

  const ystl::Vector forward (1.0f, 0.0f, 0.0f);

  // open ground: nothing blocks, nothing to jump/duck under
  CHECK (NavigateHook::BlockedForward (*bot, forward) == nullptr);
  CHECK (NavigateHook::JumpUp (*bot, forward));
  CHECK (NavigateHook::DuckUnder (*bot, forward));

  Trace::Result strafe {};
  CHECK (NavigateHook::StrafeLeft (*bot, &strafe));
  CHECK (NavigateHook::StrafeRight (*bot, &strafe));
  CHECK (!NavigateHook::BlockedLeft (*bot));
  CHECK (!NavigateHook::BlockedRight (*bot));
  CHECK (!NavigateHook::WallLeft (*bot, 40.0f));
  CHECK (!NavigateHook::WallRight (*bot, 40.0f));
  CHECK (!NavigateHook::WallBehind (*bot, 40.0f));

  // wall ahead: hull and eye rays report the hit
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = wall;
  });
  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = wall;
  });

  CHECK (NavigateHook::BlockedForward (*bot, forward) == wall);
  CHECK (!NavigateHook::StrafeLeft (*bot, &strafe));
  CHECK (!NavigateHook::StrafeRight (*bot, &strafe));
  CHECK (NavigateHook::BlockedLeft (*bot));
  CHECK (NavigateHook::BlockedRight (*bot));
  engine.AdvanceTime (0.2f);

  // doors and hostages never count as blockage (clear rays, blocked hull)
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);
  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = door;
  });
  CHECK (NavigateHook::BlockedForward (*bot, forward) == nullptr);

  engine.AdvanceTime (0.2f);
  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = hostage;
  });
  CHECK (NavigateHook::BlockedForward (*bot, forward) == nullptr); // CT skips hostages
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.2f);

  // jump needs ground, duck needs a free center lane
  bot->pev->flags &= ~FL_ONGROUND;
  CHECK (!NavigateHook::JumpUp (*bot, forward));
  bot->pev->flags |= FL_ONGROUND;

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v1;
    (void)v2;
    (void)no_monsters;
    (void)skip;
    *out = TraceResult {};
    out->flFraction = 0.5f;
  });
  CHECK (!NavigateHook::JumpUp (*bot, forward));
  CHECK (!NavigateHook::DuckUnder (*bot, forward));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // falls: flat world is safe, scripted abyss is deadly
  CHECK (!NavigateHook::DeadlyMove (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (NavigateHook::SafeToMove (*bot, ystl::Vector (200.0f, 0.0f, 0.0f))); // groundless world reads unsafe

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = v1[0] > 90.0f ? 0.05f : 1.0f;
  });
  CHECK (NavigateHook::DeadlyMove (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (!NavigateHook::SafeToMove (*bot, ystl::Vector (300.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // walkable ascent follows the step height
  CHECK (NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 10.0f)));
  CHECK (!NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 30.0f)));
  CHECK (NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (200.0f, 0.0f, 0.0f)));

  // stuck detection: speed without progress, then a real blockage
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  NavigateHook::SetMovedDistance (*bot, 0.0f);
  NavigateHook::SetPrevSpeed (*bot, 100.0f);
  CHECK (NavigateHook::StuckStatus (*bot, forward));
  CHECK (NavigateHook::Stuck (*bot));

  NavigateHook::SetMovedDistance (*bot, 1000.0f);
  NavigateHook::SetPrevSpeed (*bot, 0.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  CHECK (!NavigateHook::StuckStatus (*bot, forward));

  // slow grind accumulates into stuck after ~8 checks
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  NavigateHook::SetMovedDistance (*bot, 0.0f);
  NavigateHook::SetPrevSpeed (*bot, 0.0f);
  NavigateHook::SetStuckAccum (*bot, 0.0f);
  bool ever_stuck = false;
  for (int i = 0; i < 10; ++i) {
    ever_stuck = NavigateHook::StuckStatus (*bot, forward) || ever_stuck;
  }
  CHECK (ever_stuck);

  // breakable ahead schedules shooting instead of stuck
  // (past the post-spawn decor grace period, moving at full speed)
  bot->pev->maxspeed = 270.0f;
  engine.AdvanceTime (6.0f);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = seeded;
  });
  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = seeded;
  });
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  NavigateHook::SetMovedDistance (*bot, 1000.0f);
  CHECK (!NavigateHook::StuckStatus (*bot, forward));
  CHECK (bot->Task ()->id == TaskId::ShootBreakable);
  engine.SetTraceLineHook (nullptr);
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.2f);

  // collision weights on open ground: jump 8 + seers 5 + 5 - similar 12
  NavigateHook::SetDestOrigin (*bot, ystl::Vector (100.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  NavigateHook::SetTerrainStrafe (*bot, 1.0f, 100.0f);

  // open ground: jump 8 + seer 5 - similar 12, strafe 12 + 5 + 8 + 15,
  // left -3 + 5 - 5 (terrain bias), duck 10 - 8
  Bot::CollisionWeights weights {};
  NavigateHook::CollisionWeights (*bot, forward, weights);
  CHECK (weights[CollisionState::Jump].weight == 1);
  CHECK (weights[CollisionState::StrafeLeft].weight == -3);
  CHECK (weights[CollisionState::StrafeRight].weight == 40);
  CHECK (weights[CollisionState::Duck].weight == 2);

  // wall ahead adds the jump-over bonus on top
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    // only the low wall-check ray is blocked, clearance above stays open
    if (v1[2] < -1.0f && v2[2] < 0.0f) {
      out->flFraction = 0.5f;
      out->pHit = seeded;
    }
  });
  Bot::CollisionWeights walled {};
  NavigateHook::CollisionWeights (*bot, forward, walled);
  CHECK (walled[CollisionState::Jump].weight == 7); // +6 jump-over bonus
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // strafe speed commits to the open side
  CHECK (NavigateHook::StrafeSpeed (*bot) == 0.0f);
  NavigateHook::StrafeSpeedTo (*bot, ystl::Vector (100.0f, 0.0f, 0.0f), 100.0f);
  CHECK (NavigateHook::StrafeSpeed (*bot) == 100.0f);
  NavigateHook::StrafeSpeedTo (*bot, ystl::Vector (-100.0f, 0.0f, 0.0f), 100.0f);
  CHECK (NavigateHook::StrafeSpeed (*bot) == -100.0f);
  NavigateHook::StrafeSpeedRaw (*bot, 0.0f);
  CHECK (NavigateHook::StrafeSpeed (*bot) == -100.0f); // zero input keeps the old value

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_gaps") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // door seed for the HasDoors branches of the navigation frame
  edict_t *door = engine.SpawnEntity ("func_door");
  HOST_REQUIRE (door != nullptr);
  const float door_pos[3] = { 400.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (door, door_pos);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  CHECK (game.MapIs (MapFlags::HasDoors));

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("NavA", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  bots.Addbot ("NavB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);

  Bot *bot = nullptr, *mate = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "NavA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "NavB") {
        mate = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr);

  // fresh clients join as spectators (correct engine behavior), assign teams
  bot->team_ = Team::CT;
  mate->team_ = Team::CT;

  auto prep_bot = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
    target->pev->angles = ystl::Vector (0.0f, 0.0f, 0.0f);
    target->pev->health = 100.0f;
    target->pev->max_health = 100.0f;
    target->pev->deadflag = DEAD_NO;
    target->pev->takedamage = DAMAGE_YES;
    target->pev->solid = SOLID_BBOX;
    target->pev->movetype = MOVETYPE_WALK;
    target->pev->maxspeed = 270.0f;
    target->pev->flags |= FL_ONGROUND;
    target->is_alive_ = true;
    NavigateHook::SetHealthValue (*target, 100.0f);
    NavigateHook::SetAggression (*target, 0.9f, 0.0f);
  };
  prep_bot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_bot (mate, ystl::Vector (60.0f, 0.0f, 0.0f));

  // mate outranks us with a camp task, so avoidance tracks it
  mate->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);

  // rush timing: fresh round state reads low, personalities split...
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  CHECK (NavigateHook::Rush (*bot));

  NavigateHook::SetPersonality (*bot, Personality::Careful);
  CHECK (!NavigateHook::Rush (*bot));

  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  NavigateHook::SetHealthValue (*bot, 10.0f);
  CHECK (!NavigateHook::Rush (*bot));
  NavigateHook::SetHealthValue (*bot, 100.0f);

  NavigateHook::SetAggression (*bot, 0.1f, 0.9f);
  CHECK (!NavigateHook::Rush (*bot));
  NavigateHook::SetAggression (*bot, 0.9f, 0.0f);
  NavigateHook::SetPersonality (*bot, Personality::Normal);

  // ...lost bots refind themselves through the navigation frame...
  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);
  CHECK (NavigateHook::DestOrigin (*bot) == NavigateHook::PathOrigin (*bot));

  // ...find rebuilds the walk synchronously, the throttle holds the
  // immediate second request...
  NavigateHook::FindP (*bot, 0, 3);
  HOST_REQUIRE (NavigateHook::WalkLength (*bot) == 4);
  CHECK (NavigateHook::WalkAt (*bot, 3) == 3);

  NavigateHook::FindP (*bot, 0, 2);
  CHECK (NavigateHook::WalkLength (*bot) == 4); // throttled, old walk kept

  engine.AdvanceTime (0.15f);
  NavigateHook::FindP (*bot, 0, 2);
  HOST_REQUIRE (NavigateHook::WalkLength (*bot) == 3);
  CHECK (NavigateHook::WalkAt (*bot, 2) == 2);

  // ...clearSearchNodes drops the current walk...
  bot->ClearSearchNodes ();
  CHECK (NavigateHook::WalkLength (*bot) == 0);

  // ...terrain probes on open ground, then commits duck past the timer...
  engine.AdvanceTime (0.7f); // outlast any collision cooldown from setup
  NavigateHook::SetDestOrigin (*bot, ystl::Vector (100.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  NavigateHook::SetMovedDistance (*bot, 0.0f);
  NavigateHook::SetPrevSpeed (*bot, 100.0f);
  NavigateHook::SetTerrainStrafe (*bot, 1.0f, 100.0f);
  bot->pev->button = 0;

  NavigateHook::CheckTerr (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::CollState (*bot) == CollisionState::Probing);
  CHECK (NavigateHook::CollIndex (*bot) == 0);

  engine.AdvanceTime (0.5f);
  NavigateHook::CheckTerr (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::CollIndex (*bot) == 1);
  CHECK ((bot->pev->button & IN_DUCK) != 0);

  // ...queued responses execute directly: jump and full strafe...
  // (ground hook: the harness world is bottomless, raw strafe checks it)
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < v1[2]) {
      out->flFraction = 0.05f; // ground right below, walls stay clear
    }
  });
  engine.AdvanceTime (0.15f); // expire the trace cache before the hooked rays
  bot->pev->button = 0;
  NavigateHook::SetCollIndex (*bot, 2); // sorted third is Jump here
  NavigateHook::ExecuteColl (*bot);
  CHECK ((bot->pev->button & IN_JUMP) != 0);

  NavigateHook::SetCollIndex (*bot, 0); // sorted first is StrafeRight
  NavigateHook::ExecuteColl (*bot);
  CHECK (NavigateHook::StrafeSpeed (*bot) == bot->pev->maxspeed);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  NavigateHook::ResetColl (*bot);
  CHECK (NavigateHook::CollState (*bot) == CollisionState::Undecided);
  CHECK (NavigateHook::CollIndex (*bot) == 0);

  NavigateHook::IgnoreColl (*bot);
  CHECK (NavigateHook::CollState (*bot) == CollisionState::Undecided);
  CHECK (NavigateHook::Stuck (*bot)); // ignore drops the state, not the flag

  // ...falls track on the floor, trigger airborne, evaluate on landing...
  prep_bot (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 3);
  NavigateHook::CheckF (*bot);
  CHECK (!NavigateHook::Falling (*bot));

  bot->pev->flags &= ~FL_ONGROUND;
  bot->pev->flFallVelocity = 150.0f;
  NavigateHook::CheckF (*bot);
  CHECK (NavigateHook::Falling (*bot));

  bot->pev->origin = ystl::Vector (200.0f, 0.0f, -150.0f);
  bot->pev->flags |= FL_ONGROUND;
  engine.AdvanceTime (0.1f);
  NavigateHook::CheckF (*bot);
  CHECK (!NavigateHook::Falling (*bot));
  CHECK (bot->GetCurrentNodeIndex () == 2); // node 3 unreachable, refound below
  prep_bot (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 2);

  // ...occupancy and reachability read the client list and geometry...
  clients.Update ();
  clients[mate->Ent ()].team = Team::CT;
  clients[bot->Ent ()].team = Team::CT;

  CHECK (!NavigateHook::Occupied (*bot, 2, true));
  CHECK (NavigateHook::Reachable (*bot, 2));
  CHECK (!NavigateHook::Reachable (*bot, 4)); // island beyond 600
  CHECK (!NavigateHook::Reachable (*bot, 99));

  mate->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (NavigateHook::Occupied (*bot, 2, true));
  CHECK (!NavigateHook::Occupied (*bot, 3, true));
  CHECK (NavigateHook::Occupied (*bot, 99, true)); // invalid reads occupied

  mate->pev->origin = ystl::Vector (60.0f, 0.0f, 0.0f);
  clients.Update ();

  // ...ladder memory and alternative selection stay negative here...
  CHECK (!NavigateHook::PrevLadder (*bot));
  NavigateHook::SetPrevNode (*bot, 0, 2);
  graph.paths_[2].flags |= NodeFlag::Ladder;
  CHECK (NavigateHook::PrevLadder (*bot));
  graph.paths_[2].flags &= ~NodeFlag::Ladder;
  NavigateHook::SetPrevNode (*bot, 0, kInvalidNodeIndex);

  bot->pev->movetype = MOVETYPE_FLY; // on-ladder shortcut
  CHECK (!NavigateHook::SelectNext (*bot));
  bot->pev->movetype = MOVETYPE_WALK;

  ystl::Array<int32_t> alt_walk {};
  alt_walk.push (2);
  alt_walk.push (3);
  NavigateHook::SetWalk (*bot, alt_walk);
  CHECK (!NavigateHook::SelectNext (*bot)); // current node free, nothing to do

  // ...a blocked current node reroutes through a visible shortcut...
  // (mate camps node 1, the 0 -> 3 -> 2 detour satisfies every gate)
  for (auto &slot : graph.paths_[0].links) {
    if (slot.index == kInvalidNodeIndex) {
      slot.index = 3;
      slot.distance = 300;
      break;
    }
  }

  for (auto &slot : graph.paths_[3].links) {
    if (slot.index == kInvalidNodeIndex) {
      slot.index = 0;
      slot.distance = 300;
      break;
    }
  }
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();

  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());
  HOST_REQUIRE (vistab.Visible (0, 3) && vistab.Visible (3, 2));

  NavigateHook::ChangeNode (*bot, 0);

  ystl::Array<int32_t> detour_walk {};
  detour_walk.push (1);
  detour_walk.push (2);
  NavigateHook::SetWalk (*bot, detour_walk);
  CHECK (NavigateHook::SelectNext (*bot));
  CHECK (NavigateHook::WalkAt (*bot, 0) == 3);

  // ...avoidance commits to a strafe away from the higher-priority mate...
  // (low maxspeed keeps the movement prediction near the body)
  bot->pev->maxspeed = 100.0f;
  mate->pev->origin = ystl::Vector (180.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetStrafe (*bot, 0.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*bot) == mate->Ent ());
  CHECK (NavigateHook::StrafeSpeed (*bot) != 0.0f);
  bot->pev->maxspeed = 270.0f;

  cv_has_team_semiclip.Set (1);
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  cv_has_team_semiclip.Set (0);

  // ...jump ballistics solve level arcs and reject the impossible...
  const ystl::Vector arc = NavigateHook::CalcJump (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f));
  CHECK (arc.length2d () > 0.0f);
  CHECK (arc.z == 0.0f);
  CHECK (NavigateHook::CalcJump (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 100000.0f)).empty ());

  // ...zero gravity never jumps...
  sv_gravity.Set (0);
  CHECK (NavigateHook::CalcJump (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)).empty ());
  sv_gravity.Set (800);

  // ...path origins stay put on plain nodes and jitter inside radii...
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetPathOr (*bot);
  CHECK (NavigateHook::PathOrigin (*bot) == graph[2].origin);

  ystl::Array<int32_t> jitter_walk {};
  jitter_walk.push (3);
  NavigateHook::SetWalk (*bot, jitter_walk);
  NavigateHook::ChangeNode (*bot, 3);
  graph.paths_[3].radius = 32.0f;
  NavigateHook::SetPathOr (*bot);
  CHECK (NavigateHook::PathOrigin (*bot).distance (graph[3].origin) <= 32.0f);
  graph.paths_[3].radius = 0.0f;

  // ...the navigation frame jumps flagged links, waits on far doors,
  // and arrives home with goal practice...
  prep_bot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 0);
  bot->ClearSearchNodes ();
  bot->StartTask (TaskId::MoveTo, 50.0f, 3, 0.0f, false);
  NavigateHook::SetTravelFlags (*bot, PathFlag::Jump);
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK ((bot->pev->button & IN_JUMP) != 0);
  NavigateHook::SetTravelFlags (*bot, static_cast<PathFlag> (0));

  prep_bot (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, jitter_walk);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v1;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[0] > 150.0f) {
      out->flFraction = 0.5f;
      out->pHit = door; // short ray still ends at the far door
    }
  });
  engine.AdvanceTime (0.15f); // expire the trace cache before the hooked rays
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot)); // task target 3, not arrived
  CHECK (!NavigateHook::AimDirCan (*bot)); // door leg cleared aim direction
  CHECK (bot->pev->button == 0); // far door never pressed
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  bot->ClearSearchNodes ();
  bot->StartTask (TaskId::MoveTo, 50.0f, 2, 0.0f, false);
  NavigateHook::SetChosenGoal (*bot, 2);
  NavigateHook::SetHealthValue (*bot, 100.0f);
  CHECK (NavigateHook::UpdateNav (*bot));
  CHECK (NavigateHook::CollState (*bot) == CollisionState::Undecided);
  NavigateHook::SetChosenGoal (*bot, kInvalidNodeIndex);

  // ...lift legs: closed doors ask for buttons, entering times out...
  edict_t *plat = engine.SpawnEntity ("func_plat");
  HOST_REQUIRE (plat != nullptr);
  const float plat_pos[3] = { 200.0f, 0.0f, -50.0f };
  engine.Funcs ().pfnSetOrigin (plat, plat_pos);

  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, jitter_walk);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v1;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < -10.0f) {
      out->flFraction = 0.5f;
      out->pHit = plat; // lift shaft below the node
    }
    else if (v2[0] > 300.0f) {
      out->flFraction = 0.5f;
      out->pHit = door; // closed door on the way
    }
  });
  engine.AdvanceTime (0.15f); // expire the trace cache before the hooked rays

  CHECK (NavigateHook::LiftH (*bot));

  // entered the shaft and proceeded to the inside-button search...
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonInside));
  CHECK (NavigateHook::LiftEntity (*bot) == plat);

  engine.AdvanceTime (10.5f);
  CHECK (!NavigateHook::LiftS (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::None));
  CHECK (NavigateHook::LiftEntity (*bot) == nullptr);
  CHECK (NavigateHook::WalkLength (*bot) == 0);

  // ...travelers extend while still riding, then the round clock stops rush...
  graph.paths_[2].flags |= NodeFlag::Lift;
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, jitter_walk);
  CHECK (NavigateHook::LiftH (*bot));
  HOST_REQUIRE (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonInside));
  NavigateHook::SetLiftState (*bot, LiftState::TravelingBy);
  bot->pev->groundentity = plat;

  engine.AdvanceTime (10.5f); // outlast the inside-button window
  CHECK (NavigateHook::LiftS (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::TravelingBy));
  CHECK (NavigateHook::LiftEntity (*bot) == plat);

  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);
  NavigateHook::SetLiftState (*bot, LiftState::None);
  NavigateHook::SetLiftEntity (*bot, nullptr);
  bot->pev->groundentity = nullptr;
  graph.paths_[2].flags &= ~NodeFlag::Lift;

  game_state.RoundStart ();
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  CHECK (!NavigateHook::Rush (*bot));

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_cover") {
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

  // plain chain: the Bot ctor sizes its planner from it
  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("CoverBot", ystl::Vector (100.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);

  // no node yet: reselection picks a reachable one on the chain
  HOST_REQUIRE (bot->GetCurrentNodeIndex () == kInvalidNodeIndex);
  NavigateHook::FindValid (*bot);
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);

  // ...with an expired travel timer it re-rates the nodes and stays valid
  NavigateHook::ChangeNode (*bot, 2);
  engine.AdvanceTime (5.0f);
  NavigateHook::FindValid (*bot);
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);

  // ...and an overflowing rechoice counter rethinks the goal instead
  NavigateHook::SetHasC4 (*bot, true);
  engine.AdvanceTime (5.0f);
  NavigateHook::FindValid (*bot);
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);
  NavigateHook::SetHasC4 (*bot, false);

  ystl::Array<int32_t> empty_walk {};
  ystl::Array<int32_t> two_walk {};
  two_walk.push (1);
  two_walk.push (2);
  ystl::Array<int32_t> three_walk {};
  three_walk.push (2);
  three_walk.push (3);

  // advance with an empty walk just validates the node and stops
  NavigateHook::SetWalk (*bot, empty_walk);
  CHECK (!NavigateHook::Advance (*bot));

  // ladder lookahead: plain next node, then an empty walk
  NavigateHook::SetWalk (*bot, two_walk);
  CHECK (NavigateHook::LadderDistance (*bot) == ystl::sqrf (48.0f));
  NavigateHook::SetWalk (*bot, empty_walk);
  CHECK (NavigateHook::LadderDistance (*bot) == ystl::sqrf (15.0f));

  // ...descending towards a ladder node asks for the ladder distance
  graph.paths_[1].origin.z = 50.0f;
  graph.paths_[2].flags |= NodeFlag::Ladder;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SetWalk (*bot, two_walk);
  NavigateHook::SetPrevNode (*bot, 0, 1);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 25.0f);
  CHECK (NavigateHook::LadderDistance (*bot) == ystl::sqrf (72.0f));
  graph.paths_[1].origin.z = 0.0f;
  graph.paths_[2].flags &= ~NodeFlag::Ladder;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);

  // no lift under the node: handler has nothing to do, returns true
  // (= continue navigation, only false aborts/repaths)
  NavigateHook::ChangeNode (*bot, 0);
  CHECK (NavigateHook::LiftH (*bot));

  // flying over a node without ladders keeps the exact origin...
  bot->pev->movetype = MOVETYPE_FLY;
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetPathOr (*bot);
  CHECK (NavigateHook::PathOrigin (*bot) == graph[2].origin);
  bot->pev->movetype = MOVETYPE_WALK;

  // ...a wide node jitters inside its radius around the walk...
  graph.paths_[2].radius = 32.0f;
  NavigateHook::SetWalk (*bot, three_walk);
  CHECK (NavigateHook::WalkLength (*bot) == 2);
  NavigateHook::SetPathOr (*bot);
  CHECK (NavigateHook::PathOrigin (*bot).distance (graph[2].origin) <= 32.0f);

  // ...while a narrow node skips the circular spread and uses the
  // body-angle forward jitter instead (still bounded by the radius)
  graph.paths_[2].flags |= NodeFlag::Narrow;
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, three_walk);
  NavigateHook::SetPathOr (*bot);
  CHECK (NavigateHook::PathOrigin (*bot).distance (graph[2].origin) <= 32.0f);
  graph.paths_[2].flags &= ~NodeFlag::Narrow;
  graph.paths_[2].radius = 0.0f;

  // flying towards the goal pushes forward...
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, empty_walk);
  bot->pev->movetype = MOVETYPE_FLY;
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_FORWARD) != 0);

  // ...walking a plain node leaves the buttons alone...
  auto walker = CreateNavBot ("WalkBot", ystl::Vector (100.0f, 0.0f, 0.0f));
  HOST_REQUIRE (walker.get () != nullptr);
  NavigateHook::SetDestOrigin (*walker, ystl::Vector (300.0f, 0.0f, 0.0f));
  walker->pev->button = 0;
  NavigateHook::MoveToGoal (*walker);
  CHECK (walker->pev->button == 0);

  // ...an airy crouch node never ducks...
  graph.paths_[2].flags |= NodeFlag::Crouch;
  NavigateHook::ChangeNode (*walker, 2);
  walker->pev->button = 0;
  NavigateHook::MoveToGoal (*walker);
  CHECK (walker->pev->button == 0);
  graph.paths_[2].flags &= ~NodeFlag::Crouch;

  // ...while swimming steers by view and pitch
  walker->pev->waterlevel = 2;
  NavigateHook::SetDestOrigin (*walker, ystl::Vector (300.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveAngles (*walker, ystl::Vector (0.0f, 0.0f, 0.0f));
  walker->pev->button = 0;
  NavigateHook::MoveToGoal (*walker);
  CHECK ((walker->pev->button & IN_FORWARD) != 0);

  NavigateHook::SetDestOrigin (*walker, ystl::Vector (-500.0f, 0.0f, 0.0f));
  walker->pev->button = 0;
  NavigateHook::MoveToGoal (*walker);
  CHECK ((walker->pev->button & IN_BACK) != 0);

  NavigateHook::SetDestOrigin (*walker, ystl::Vector (300.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveAngles (*walker, ystl::Vector (70.0f, 0.0f, 0.0f));
  walker->pev->button = 0;
  NavigateHook::MoveToGoal (*walker);
  CHECK ((walker->pev->button & IN_DUCK) != 0);

  NavigateHook::SetMoveAngles (*walker, ystl::Vector (-70.0f, 0.0f, 0.0f));
  walker->pev->button = 0;
  NavigateHook::MoveToGoal (*walker);
  CHECK ((walker->pev->button & IN_JUMP) != 0);
  walker->pev->waterlevel = 0;

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_ladder_strafe") {
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

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("LadderBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);

  // latched ground strafe must not express while climbing
  bot->pev->movetype = MOVETYPE_FLY;
  NavigateHook::SetStrafe (*bot, 250.0f);
  bot->pev->button = 0;
  NavigateHook::TranslateInput (*bot);
  CHECK (NavigateHook::StrafeSpeed (*bot) == 0.0f);
  CHECK ((bot->pev->button & (IN_MOVELEFT | IN_MOVERIGHT)) == 0);

  // control: on the ground the same strafe drives buttons
  bot->pev->movetype = MOVETYPE_WALK;
  NavigateHook::SetStrafe (*bot, 250.0f);
  bot->pev->button = 0;
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_MOVERIGHT) != 0);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_bomb") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // demolition map for the planted-bomb branches below
  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  CHECK (game.MapIs (MapFlags::Demolition));

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("BombBot", ystl::Vector (700.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);

  // no bomb anywhere: falls back to the node nearest the empty origin
  CHECK (NavigateHook::BombNode (*bot) == 0);

  // plant between nodes 1 (100) and 2 (200)
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (140.0f, 0.0f, 0.0f));

  // standing next to it resolves the closest node directly...
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  CHECK (NavigateHook::BombNode (*bot) == 1);

  // ...hearing it from afar resolves the same node through the sound
  bot->pev->origin = ystl::Vector (700.0f, 0.0f, 0.0f);
  CHECK (NavigateHook::BombNode (*bot) == 1);

  // the planted entity itself maps to that node as well
  edict_t *planted = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (planted != nullptr);
  planted->v.model = string_t (engine.AllocString ("models/w_c4.mdl"));

  const float planted_pos[3] = { 140.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (planted, planted_pos);

  CHECK (NavigateHook::NearestPlanted (*bot) == 1);

  // multi-site regression: with far-apart bomb sites the nearby unvisited site
  // must stay selectable, or the bot ping-pongs and stands at the midpoint
  game_state.SetBombPlanted (false);
  graph.Reset ();

  const ystl::Vector multi_spots[3] = { ystl::Vector (100.0f, 0.0f, 0.0f), ystl::Vector (2100.0f, 0.0f, 0.0f),
    ystl::Vector (4100.0f, 0.0f, 0.0f) };
  const int32_t multi_flags[3] = { ystl::to_underlying (NodeFlag::Goal), 0, ystl::to_underlying (NodeFlag::Goal) };

  for (int i = 0; i < 3; ++i) {
    Path path {};
    path.origin = multi_spots[i];
    path.number = i;
    path.flags = multi_flags[i];
    path.light = kInvalidLightLevel;

    for (auto &slot : path.links) {
      slot.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto multi_link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };
  multi_link (0, 1, 2000);
  multi_link (1, 0, 2000);
  multi_link (1, 2, 2000);
  multi_link (2, 1, 2000);

  graph.PopulateNodes ();
  planner.Init ();

  HOST_REQUIRE (graph.GetPoints (PointType::Goal).size () == 2);

  // bomb on the far site (node 0), bot standing on the near site (node 2)
  bot->pev->origin = ystl::Vector (4100.0f, 0.0f, 0.0f);
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (100.0f, 0.0f, 0.0f));

  // both sites unvisited: the near one must remain selectable. filtering it out
  // (distance to bot) made the bot head to the far site and ping-pong halfway
  for (int i = 0; i < 5; ++i) {
    CHECK (NavigateHook::BombNode (*bot) == 2);
  }

  // area marking: a goal point on the same site as the checked one is covered too
  Path secondary {};
  secondary.origin = ystl::Vector (4500.0f, 0.0f, 0.0f); // 400 from node 2, same site
  secondary.number = graph.Length ();
  secondary.flags = ystl::to_underlying (NodeFlag::Goal);
  secondary.light = kInvalidLightLevel;

  for (auto &slot : secondary.links) {
    slot.index = kInvalidNodeIndex;
  }
  graph.paths_.push (secondary);
  graph.PopulateNodes ();

  graph.ClearVisited ();
  NavigateHook::MarkSite (*bot, 2);
  CHECK (graph.IsVisited (2));
  CHECK (graph.IsVisited (3));
  CHECK (!graph.IsVisited (0));

  game_state.SetBombPlanted (false);
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_sealed_bomb") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // demolition map, so the planted-bomb branches are live
  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("SealedBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->team_ = Team::CT;

  // bomb planted inside node 2, bot outside at node 0, wall in between at x=150
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (200.0f, 0.0f, 0.0f));

  mode_walls.Reset ();
  CHECK (!mode_walls.HasWalls ());
  CHECK (!NavigateHook::TryTeleportToSealedBomb (*bot));
  CHECK (bot->pev->origin.distance (ystl::Vector (0.0f, 0.0f, 0.0f)) < 1.0f);

  edict_t *wall = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (wall != nullptr);
  wall->v.solid = SOLID_BBOX;
  wall->v.absmin = ystl::Vector (140.0f, -200.0f, -100.0f);
  wall->v.absmax = ystl::Vector (160.0f, 200.0f, 100.0f);
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());

  // the straight line to the bomb is sealed, even though the bot is only 200 units away
  HOST_REQUIRE (mode_walls.IsSegmentBlocked (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (200.0f, 0.0f, 0.0f)));

  CHECK (NavigateHook::TryTeleportToSealedBomb (*bot));
  CHECK (bot->pev->origin.x > 150.0f); // warped to the far side of the wall
  CHECK (bot->pev->origin.distance_sq (ystl::Vector (200.0f, 0.0f, 0.0f)) < ystl::sqrf (256.0f));

  // already inside, next tick must not warp again
  CHECK (!NavigateHook::TryTeleportToSealedBomb (*bot));

  game_state.SetBombPlanted (false);
  mode_walls.Reset ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_goal_variants") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  // flagged chain matching the goals fixture
  graph.Reset ();

  const ystl::Vector spots[6] = {
    ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::Vector (300.0f, 0.0f, 0.0f),
    ystl::Vector (400.0f, 0.0f, 0.0f),
    ystl::Vector (500.0f, 0.0f, 0.0f),
  };
  const int32_t flag_of[6] = {
    0,
    ystl::to_underlying (NodeFlag::TerroristOnly),
    ystl::to_underlying (NodeFlag::CTOnly),
    ystl::to_underlying (NodeFlag::Goal),
    ystl::to_underlying (NodeFlag::Camp),
    ystl::to_underlying (NodeFlag::Rescue),
  };

  for (int i = 0; i < 6; ++i) {
    Path path {};
    path.origin = spots[i];
    path.number = i;
    path.flags = flag_of[i];
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  for (int i = 0; i < 5; ++i) {
    link (i, i + 1, 100);
    link (i + 1, i, 100);
  }
  graph.PopulateNodes ();
  planner.Init ();

  auto bot = CreateNavBot ("GoalXBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->team_ = Team::Terrorist;
  bot->agression_level_ = 0.5f;
  bot->fear_level_ = 0.5f;

  // zombie creatures take the terrorist pool first
  game.AddGameFlag (GameFlags::ZombieMod);
  NavigateHook::SetCreature (*bot, true);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  game.ClearGameFlag (GameFlags::ZombieMod);
  NavigateHook::SetCreature (*bot, false);

  // personalities reshape the same call
  bot->personality_ = Personality::Rusher;
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  bot->personality_ = Personality::Careful;
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  bot->personality_ = Personality::Normal;
  NavigateHook::Rng (*bot).force_float (0.0f);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();

  // carriers and vip take the goal post directly
  NavigateHook::SetHasC4 (*bot, true);
  CHECK (NavigateHook::BestGoal (*bot) == 3);
  NavigateHook::SetHasC4 (*bot, false);

  bot->is_vip_ = true;
  CHECK (NavigateHook::BestGoal (*bot) == 3);
  bot->is_vip_ = false;

  bot->team_ = Team::CT;
  bot->has_hostage_ = true;
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  bot->has_hostage_ = false;

  // planted bomb on a demolition map pulls CTs to the bomb node
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (300.0f, 0.0f, 0.0f));
  bot->team_ = Team::CT;
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // terrorists guard it instead when the timer allows
  bot->team_ = Team::Terrorist;
  NavigateHook::SetDefendedBomb (*bot, false);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  game_state.SetBombPlanted (false);

  // tactic coin flips go both ways under a pinned roll
  NavigateHook::Rng (*bot).force_chance (true);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).force_chance (false);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();

  // camp guns keep camping, others drop it; snipers inflate it
  bot->weapon_type_ = WeaponType::Pistol;
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  bot->weapon_type_ = WeaponType::Sniper;
  bot->current_weapon_ = Weapon::AWP;
  NavigateHook::Rng (*bot).force_float (2.0f);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();

  bot->weapon_type_ = WeaponType::Rifle;
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_defend_cover") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("DefendBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->team_ = Team::CT;
  NavigateHook::ChangeNode (*bot, 0);

  // no visibility data: defend falls back to a random node, cover goes blind
  CHECK (graph.Exists (NavigateHook::DefendNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f))));
  NavigateHook::SetEnemyOrigin (*bot, ystl::Vector (400.0f, 0.0f, 0.0f));
  CHECK (graph.Exists (NavigateHook::CoverNode (*bot, 1000.0f)));

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_camp_aim") {
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

  BuildNavGraph ();
  planner.Init ();

  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());

  auto bot = CreateNavBot ("CampBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  NavigateHook::ChangeNode (*bot, 0);

  // camp direction picks a visible far node
  NavigateHook::Rng (*bot).force_int (0);
  CHECK (graph.Exists (NavigateHook::CampDir (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();

  // aiming resolves along a visible path
  int aim_length = 0;
  CHECK (graph.Exists (NavigateHook::AimingNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f), aim_length)));

  // strafe speed respects walls on each side
  NavigateHook::StrafeSpeedRaw (*bot, 100.0f);
  NavigateHook::StrafeSpeedRaw (*bot, -100.0f);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_bulk") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  edict_t *door = engine.SpawnEntity ("func_door");
  HOST_REQUIRE (door != nullptr);

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
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("BulkA", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  bots.Addbot ("BulkB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);

  Bot *bot = nullptr, *mate = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "BulkA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "BulkB") {
        mate = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr);

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;

  auto prep_bulk = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
    target->pev->angles = ystl::Vector (0.0f, 0.0f, 0.0f);
    target->pev->health = 100.0f;
    target->pev->max_health = 100.0f;
    target->pev->deadflag = DEAD_NO;
    target->pev->takedamage = DAMAGE_YES;
    target->pev->solid = SOLID_BBOX;
    target->pev->movetype = MOVETYPE_WALK;
    target->pev->maxspeed = 270.0f;
    target->pev->flags |= FL_ONGROUND;
    target->is_alive_ = true;
    NavigateHook::SetHealthValue (*target, 100.0f);
    NavigateHook::SetAggression (*target, 0.9f, 0.0f);
  };
  prep_bulk (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_bulk (mate, ystl::Vector (60.0f, 0.0f, 0.0f));

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  clients.Update ();
  clients[bot->Ent ()].team = Team::CT;
  clients[mate->Ent ()].team = Team::CT;

  const ystl::Vector forward (1.0f, 0.0f, 0.0f);

  // collision lifecycle: reset, ignore, execute, weights
  NavigateHook::ResetColl (*bot);
  NavigateHook::IgnoreColl (*bot);
  Bot::CollisionWeights weights {};
  NavigateHook::CollisionWeights (*bot, forward, weights);
  NavigateHook::ExecuteColl (*bot);

  // every collision response fires from its slot
  NavigateHook::SetCollIndex (*bot, 99);
  NavigateHook::ExecuteColl (*bot); // out of range, silent

  NavigateHook::SetCollIndex (*bot, 0);
  bot->pev->button = 0;
  NavigateHook::SetCollMove (*bot, 0, CollisionState::Jump);
  NavigateHook::ExecuteColl (*bot);
  CHECK ((bot->pev->button & IN_JUMP) != 0);

  bot->pev->button = 0;
  NavigateHook::SetCollMove (*bot, 0, CollisionState::Duck);
  NavigateHook::ExecuteColl (*bot);
  CHECK ((bot->pev->button & IN_DUCK) != 0);

  NavigateHook::SetCollMove (*bot, 0, CollisionState::StrafeLeft);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < v1[2] - 500.0f) {
      out->flFraction = 0.05f; // ground below, sideways rays stay clean
    }
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::ExecuteColl (*bot);
  CHECK (NavigateHook::StrafeSpeed (*bot) == -bot->pev->maxspeed);

  NavigateHook::SetCollMove (*bot, 0, CollisionState::StrafeRight);
  NavigateHook::ExecuteColl (*bot);
  CHECK (NavigateHook::StrafeSpeed (*bot) == bot->pev->maxspeed);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // avoidance tracks a camping mate, then releases it
  mate->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  NavigateHook::Avoid (*bot, forward);
  CHECK (NavigateHook::Hindrance (*bot) != nullptr);

  mate->ClearTasks ();
  NavigateHook::Avoid (*bot, forward);

  // stuck status with and without movement
  CHECK (!NavigateHook::StuckStatus (*bot, forward));
  bot->pev->velocity = ystl::Vector (100.0f, 0.0f, 0.0f);
  NavigateHook::StuckStatus (*bot, forward);

  // terrain, fall, ladder distance run without crashing
  NavigateHook::CheckTerr (*bot, forward);
  NavigateHook::CheckF (*bot);
  CHECK (NavigateHook::Falling (*bot) == false);
  NavigateHook::LadderDistance (*bot);

  // movement primitives run through
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::MoveToGoal (*bot);
  NavigateHook::TranslateInput (*bot);
  NavigateHook::ResetMovement (*bot);
  NavigateHook::ReachTime (*bot);

  // deadly and safe predicates on open ground
  CHECK (!NavigateHook::DeadlyMove (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (NavigateHook::SafeToMove (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (10.0f, 0.0f, 0.0f)));

  // alternative selection bails early on ladders, free or invalid nodes
  bot->pev->movetype = MOVETYPE_FLY;
  CHECK (!NavigateHook::SelectNext (*bot));
  bot->pev->movetype = MOVETYPE_WALK;

  ystl::Array<int32_t> alt_walk {};
  alt_walk.push (1);
  alt_walk.push (2);
  NavigateHook::SetWalk (*bot, alt_walk);
  CHECK (!NavigateHook::SelectNext (*bot)); // current node free, nothing to do

  NavigateHook::ChangeNode (*mate, 1);
  mate->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  CHECK (!NavigateHook::SelectNext (*bot)); // occupied but no prev to detour from
  mate->ClearTasks ();

  // node validity refresh paths
  NavigateHook::FindValid (*bot);
  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  NavigateHook::FindValid (*bot);

  // input translation respects cvars, timers and speeds
  cv_dont_shoot.Set (1);
  bot->pev->button = IN_ATTACK;
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_ATTACK) == 0);
  cv_dont_shoot.Set (0);

  bot->pev->button = IN_JUMP;
  NavigateHook::TranslateInput (*bot);
  bot->pev->button = 0;
  bot->pev->flags &= ~FL_ONGROUND;
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_DUCK) != 0);
  bot->pev->flags |= FL_ONGROUND;

  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  bot->pev->button = 0;
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_FORWARD) != 0);

  NavigateHook::SetMoveSpeed (*bot, -100.0f);
  bot->pev->button = 0;
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_BACK) != 0);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);

  NavigateHook::SetStrafe (*bot, 50.0f);
  bot->pev->button = 0;
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_MOVERIGHT) != 0);

  NavigateHook::SetStrafe (*bot, -50.0f);
  bot->pev->button = 0;
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_MOVELEFT) != 0);
  NavigateHook::SetStrafe (*bot, 0.0f);

  // ascent rejects zero steps and sudden lips
  sv_stepsize.Set (0);
  CHECK (!NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (10.0f, 0.0f, 0.0f)));
  sv_stepsize.Set (18);

  // lift leaving arms the leaving timeout
  edict_t *lift_plat = engine.SpawnEntity ("func_plat");
  HOST_REQUIRE (lift_plat != nullptr);
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SetLiftEntity (*bot, lift_plat);
  NavigateHook::SetLiftState (*bot, LiftState::TravelingBy);
  CHECK (NavigateHook::LiftS (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::Leaving));

  // every lift state runs through the handler without crashing
  for (int s = 1; s <= 7; ++s) {
    NavigateHook::SetLiftState (*bot, static_cast<LiftState> (s));
    NavigateHook::SetLiftEntity (*bot, lift_plat);
    NavigateHook::LiftH (*bot);
  }
  NavigateHook::SetLiftState (*bot, LiftState::None);
  NavigateHook::SetLiftEntity (*bot, nullptr);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_locomotion") {
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

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("MoveXBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->pev->maxspeed = 270.0f;
  bot->pev->flags |= FL_ONGROUND;
  bot->team_ = Team::CT;

  // crouch nodes duck only against a real low ceiling
  graph.paths_[0].flags |= NodeFlag::Crouch;
  NavigateHook::ChangeNode (*bot, 0);
  bot->pev->button = 0;

  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_DUCK) != 0);
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.15f);

  // open headroom keeps standing
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_DUCK) == 0);

  // swimming drives by facing and pitch
  bot->pev->waterlevel = 3;
  bot->pev->v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  NavigateHook::SetDestOrigin (*bot, ystl::Vector (100.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & (IN_FORWARD | IN_BACK)) != 0);
  bot->pev->waterlevel = 0;

  // ladders always push forward
  bot->pev->movetype = MOVETYPE_FLY;
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_FORWARD) != 0);
  bot->pev->movetype = MOVETYPE_WALK;

  graph.paths_[0].flags &= ~NodeFlag::Crouch;

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_nodes_extra") {
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

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("NodeXBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->team_ = Team::CT;

  // reach time answers from flags, fire, damage and geometry
  NavigateHook::ChangeNode (*bot, 1);
  CHECK (NavigateHook::ReachTime (*bot) > 0.0f);

  // validity refresh with and without a node
  NavigateHook::FindValid (*bot);
  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  NavigateHook::FindValid (*bot);
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);

  // planted-bomb lookup stays empty without a bomb
  CHECK (NavigateHook::NearestPlanted (*bot) == kInvalidNodeIndex);

  // occupancy and reachability on the empty map
  CHECK (!NavigateHook::Occupied (*bot, 1, false));
  CHECK (NavigateHook::Reachable (*bot, 1));
  CHECK (!NavigateHook::PrevLadder (*bot));

  // button lookup by target, blind and plain
  edict_t *btn = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (btn != nullptr);
  btn->v.target = string_t (engine.AllocString ("door9"));
  CHECK (NavigateHook::LookupBtn (*bot, "door9", true) == btn);
  CHECK (NavigateHook::LookupBtn (*bot, "door9", false) == btn);
  CHECK (NavigateHook::LookupBtn (*bot, "none", true) == nullptr);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

} // namespace bot
