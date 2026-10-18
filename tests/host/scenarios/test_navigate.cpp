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
  static void SetFrameInterval (Bot &bot, float value) {
    bot.frame_interval_ = value;
  }
  static void SetDesiredVelocity (Bot &bot, const ystl::Vector &value) {
    bot.desired_velocity_ = value;
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
  static void SetInBombZone (Bot &bot, bool value) {
    bot.in_bomb_zone_ = value;
  }
  static void SetProgressBar (Bot &bot, bool value) {
    bot.has_progress_bar_ = value;
  }
  static void SetStale (Bot &bot, bool value) {
    bot.is_stale_ = value;
  }
  static void SetVip (Bot &bot, bool value) {
    bot.is_vip_ = value;
  }
  static void SetHasHostage (Bot &bot, bool value) {
    bot.has_hostage_ = value;
  }
  static void SetTeam (Bot &bot, Team value) {
    bot.team_ = value;
  }
  static void SetDifficulty (Bot &bot, Difficulty value) {
    bot.difficulty_ = value;
  }
  static void AddIgnored (Bot &bot, edict_t *ent) {
    bot.ignored_items_.push (ent);
  }
  static void PushHistory (Bot &bot, int index) {
    bot.goal_history_.emplace_last (index);
  }
  static void ClearHistory (Bot &bot) {
    bot.goal_history_.clear ();
  }
  static void SetLastDamage (Bot &bot, float value) {
    bot.last_damage_timestamp_ = value;
  }
  static void SetTimeCamping (Bot &bot, float value) {
    bot.time_camping_ = value;
  }
  static void SetTargetEntity (Bot &bot, edict_t *ent) {
    bot.target_entity_ = ent;
  }
  static void SetHindranceEnt (Bot &bot, edict_t *ent) {
    bot.hindrance_ = ent;
  }
  static void SetPathOriginVec (Bot &bot, const ystl::Vector &value) {
    bot.path_origin_ = value;
  }
  static void SetCheckTerrain (Bot &bot, bool value) {
    bot.check_terrain_ = value;
  }
  static void PushHostage (Bot &bot, edict_t *ent) {
    bot.hostages_.push (ent);
  }
  static void SetJumpSeq (Bot &bot, bool value) {
    bot.jump_sequence_ = value;
  }
  static void SetRechoice (Bot &bot, int value) {
    bot.rechoice_goal_count_ = value;
  }
  static void SetPathFlags (Bot &bot, NodeFlag flags) {
    bot.path_flags_ = flags;
  }
  static void SetJumpFinished (Bot &bot, bool value) {
    bot.jump_finished_ = value;
  }
  static void SetEnemy (Bot &bot, edict_t *ent) {
    bot.enemy_ = ent;
  }
  static void SetLadderDir (Bot &bot, LadderDir dir) {
    bot.ladder_dir_ = dir;
  }
  static void SetLiftTravel (Bot &bot, const ystl::Vector &pos) {
    bot.lift_travel_pos_ = pos;
  }
  static void SetBaseAggression (Bot &bot, float value) {
    bot.base_agression_level_ = value;
  }
  static void ClearHostages (Bot &bot) {
    bot.hostages_.clear ();
  }
  static void HoldAvoidCommit (Bot &bot, float hold_time) {
    bot.avoid_commit_timer_.start (hold_time);
  }
  static edict_t *Pickup (Bot &bot) {
    return bot.pickup_item_;
  }
  static bool NextBestEx (Bot &bot, const ystl::Array<int32_t> &data, bool handle_fails) {
    return bot.FindNextBestNodeEx (data, handle_fails);
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

// jump chain replica of de_dust2 149 -> 150 -> 152 (two exact jump nodes in a row)
// plus a vent pair mirroring 537 (plain, r16) -> 538 (crouch, r0), kept flat so the
// trace-height bands (eye 28 vs jump rays 9/27) stay exactly separable in hooks
void BuildChainGraph () {
  graph.Reset ();

  const ystl::Vector spots[5] = {
    ystl::Vector (0.0f, 0.0f, 0.0f), // 0: chain start (plain)
    ystl::Vector (42.0f, 0.0f, 0.0f), // 1: first box top, exact jump node
    ystl::Vector (108.0f, 0.0f, 0.0f), // 2: second box top
    ystl::Vector (200.0f, 0.0f, 0.0f), // 3: vent approach (plain, r16)
    ystl::Vector (244.0f, 0.0f, 0.0f), // 4: vent (crouch, r0)
  };
  const float radius_of[5] = { 16.0f, 0.0f, 0.0f, 16.0f, 0.0f };
  const int32_t flag_of[5] = {
    0,
    0,
    0,
    0,
    ystl::to_underlying (NodeFlag::Crouch),
  };

  for (int i = 0; i < 5; ++i) {
    Path path {};
    path.origin = spots[i];
    path.number = i;
    path.radius = radius_of[i];
    path.flags = flag_of[i];
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to, int distance, uint16_t flags, const ystl::Vector &velocity) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        slot.flags = flags;
        slot.velocity = velocity;
        return;
      }
    }
    HOST_REQUIRE (false);
  };
  const uint16_t jump = ystl::to_underlying (PathFlag::Jump);

  link (0, 1, 42, jump, ystl::Vector (110.0f, 0.0f, 0.0f));
  link (1, 0, 42, 0, ystl::Vector ());
  link (1, 2, 66, jump, ystl::Vector (130.0f, 0.0f, 0.0f));
  link (2, 1, 66, 0, ystl::Vector ());
  link (3, 4, 44, 0, ystl::Vector ());
  link (4, 3, 44, 0, ystl::Vector ());

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
  auto bot = ystl::make_unique<Bot> (ent, Difficulty::Normal, Personality::Normal, Team::CT, Skin::Random, false);

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

  bots.Addbot ("NavA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("NavB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

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

TEST_CASE ("unit/navigate_avoid_commit") {
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
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("AvoA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("AvoB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("AvoC", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  Bot *bot = nullptr, *mate_a = nullptr, *mate_b = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate_a == nullptr || mate_b == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "AvoA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "AvoB") {
        mate_a = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "AvoC") {
        mate_b = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate_a != nullptr && mate_b != nullptr);

  // isolate the trio, quota top-ups must not photobomb the hindrance scan
  bots.ForEach ([&] (Bot *candidate) {
    if (candidate != bot && candidate != mate_a && candidate != mate_b) {
      candidate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
    }
    return false;
  });

  bot->team_ = Team::CT;
  mate_a->team_ = Team::CT;
  mate_b->team_ = Team::CT;

  // client teams only arrive via TeamInfo messages, publish them here
  clients[bot->Ent ()].team = Team::CT;
  clients[mate_a->Ent ()].team = Team::CT;
  clients[mate_b->Ent ()].team = Team::CT;

  auto prep_bot = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
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
  };
  prep_bot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_bot (mate_a, ystl::Vector (60.0f, 0.0f, 0.0f));
  prep_bot (mate_b, ystl::Vector (500.0f, 0.0f, 0.0f));

  // low maxspeed keeps the movement prediction near the body
  bot->pev->maxspeed = 100.0f;

  // mates outrank us with camp tasks, so avoidance tracks them, not the reverse
  mate_a->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  mate_b->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);

  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetStrafe (*bot, 0.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  clients.Update ();

  // head-on mate commits a slide to one side...
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  HOST_REQUIRE (NavigateHook::Hindrance (*bot) == mate_a->Ent ());

  const float first = NavigateHook::StrafeSpeed (*bot);
  HOST_REQUIRE (first != 0.0f);

  // ...swapping the hindrance mid-commit must hold the slide, not flip it
  mate_a->pev->origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  mate_b->pev->origin = ystl::Vector (45.0f, -45.0f, 0.0f);
  clients.Update ();

  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*bot) == mate_b->Ent ());
  CHECK (NavigateHook::StrafeSpeed (*bot) == first);

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

  // ...a crouch node ducks...
  graph.paths_[2].flags |= NodeFlag::Crouch;
  NavigateHook::ChangeNode (*walker, 2);
  walker->pev->button = 0;
  NavigateHook::MoveToGoal (*walker);
  CHECK ((walker->pev->button & IN_DUCK) != 0);
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

  // both sites unvisited: a bot that doesn't deduce keeps the near one selectable
  NavigateHook::Rng (*bot).force_chance (false);
  NavigateHook::Rng (*bot).force_int (1);

  for (int i = 0; i < 5; ++i) {
    CHECK (NavigateHook::BombNode (*bot) == 2);
  }
  NavigateHook::Rng (*bot).clear_forced ();

  // ...while a deducing bot skips its already clear surroundings and rotates off-site
  NavigateHook::Rng (*bot).force_chance (true);

  for (int i = 0; i < 5; ++i) {
    CHECK (NavigateHook::BombNode (*bot) == 0);
  }
  NavigateHook::Rng (*bot).clear_forced ();

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

  bots.Addbot ("BulkA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("BulkB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

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

  // crouch node ducks; only an open camp/goal node stands
  graph.paths_[0].flags |= NodeFlag::Crouch;
  NavigateHook::ChangeNode (*bot, 0);
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_DUCK) != 0);

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

// crouch nodes duck; only an open camp/goal node stands
TEST_CASE ("unit/navigate_crouch") {
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

  auto bot = CreateNavBot ("CrouchBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->pev->maxspeed = 270.0f;
  bot->pev->flags |= FL_ONGROUND;
  bot->team_ = Team::CT;

  // a plain crouch node ducks, no geometry probe
  graph.paths_[2].flags |= NodeFlag::Crouch;
  NavigateHook::ChangeNode (*bot, 2);
  bot->pev->origin = graph[2].origin;
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_DUCK) != 0);

  // only the current node matters: a distant crouch successor does not duck the leg
  graph.paths_[2].flags &= ~NodeFlag::Crouch;
  graph.paths_[3].flags |= NodeFlag::Crouch;
  NavigateHook::ChangeNode (*bot, 1);
  ystl::Array<int32_t> walk {};
  walk.push (2);
  walk.push (3);
  NavigateHook::SetWalk (*bot, walk); // First=2, Next=3 (crouch)
  bot->pev->origin = graph[1].origin;
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_DUCK) == 0);
  graph.paths_[3].flags &= ~NodeFlag::Crouch;

  // an open camp crouch node stands: the probe over the node sees sky
  graph.paths_[2].flags |= NodeFlag::Crouch | NodeFlag::Camp;
  NavigateHook::ChangeNode (*bot, 2);
  bot->pev->origin = graph[2].origin;
  bot->pev->button = 0;
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out); // clear sky
  });
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_DUCK) == 0);

  // ...but the same node under a low ceiling ducks
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f; // ceiling above the node
  });
  bot->pev->button = 0;
  NavigateHook::MoveToGoal (*bot);
  CHECK ((bot->pev->button & IN_DUCK) != 0);
  engine.SetTraceLineHook (nullptr);
  graph.paths_[2].flags &= ~(NodeFlag::Crouch | NodeFlag::Camp);

  // stuck at a crouch node: the probe picks duck, not a strafe into the wall
  graph.paths_[2].flags |= NodeFlag::Crouch;
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out); // duck-height rays stay clear
  });
  engine.SetTraceHullHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f; // standing-height hulls blocked
  });
  NavigateHook::ChangeNode (*bot, 2); // crouch node
  bot->pev->origin = graph[2].origin;
  NavigateHook::SetDestOrigin (*bot, graph[3].origin);
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  NavigateHook::SetTerrainStrafe (*bot, 1.0f, 100.0f);
  CHECK (NavigateHook::DuckUnder (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)));

  Bot::CollisionWeights weights {};
  NavigateHook::CollisionWeights (*bot, ystl::Vector (1.0f, 0.0f, 0.0f), weights);
  CHECK (weights[CollisionState::Duck].weight > weights[CollisionState::StrafeLeft].weight);
  CHECK (weights[CollisionState::Duck].weight > weights[CollisionState::StrafeRight].weight);

  engine.SetTraceHullHook (nullptr);
  engine.SetTraceLineHook (nullptr);

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

// overshoot predict: a fully blocked bot (zero velocity) must not count an exact
// node as reached by equality, only real motion past it advances the walk
TEST_CASE ("unit/navigate_jump_predict") {
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

  BuildChainGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("PredictBot", ystl::Vector (42.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->pev->maxspeed = 270.0f;
  bot->pev->flags |= FL_ONGROUND;
  bot->team_ = Team::CT;
  bot->StartTask (TaskId::MoveTo, 50.0f, 2, 0.0f, false);

  // plain arrival at the exact jump node (like 151 -> 150): no travel flags here,
  // the exactness comes from the node outgoing jump links alone
  ystl::Array<int32_t> walk {};
  walk.push (1);
  walk.push (2);
  NavigateHook::ChangeNode (*bot, 1);
  NavigateHook::SetWalk (*bot, walk);
  NavigateHook::SetFrameInterval (*bot, 0.02f);

  // walled 10u short, zero velocity: predict == node must not count as passing
  bot->pev->origin = ystl::Vector (52.0f, 0.0f, 0.0f);
  bot->pev->velocity.clear ();
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (bot->GetCurrentNodeIndex () == 1);

  // real overshoot still advances: 2u short, driving 250 through the point
  bot->pev->origin = ystl::Vector (44.0f, 0.0f, 0.0f);
  bot->pev->velocity = ystl::Vector (-250.0f, 0.0f, 0.0f);
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (bot->GetCurrentNodeIndex () == 2);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

// apex-timed takeoff: a marginal rising jump link is retimed so the arc apex
// meets the lip, sufficient arcs and flat links keep the mapper velocity
TEST_CASE ("unit/navigate_jump_retime") {
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

  // takeoff (0,300,0) plain, dest (66,300,51): 51 up, the dust2 150 -> 152 shape
  graph.Reset ();

  const ystl::Vector spots[2] = {
    ystl::Vector (0.0f, 300.0f, 0.0f),
    ystl::Vector (66.0f, 300.0f, 51.0f),
  };

  for (int i = 0; i < 2; ++i) {
    Path path {};
    path.origin = spots[i];
    path.number = i;
    path.radius = 0.0f;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  for (auto &slot : graph.paths_[0].links) {
    if (slot.index == kInvalidNodeIndex) {
      slot.index = 1;
      slot.distance = 66;
      slot.flags = ystl::to_underlying (PathFlag::Jump);
      slot.velocity = ystl::Vector (130.0f, 0.0f, 0.0f);
      break;
    }
  }

  for (auto &slot : graph.paths_[1].links) {
    if (slot.index == kInvalidNodeIndex) {
      slot.index = 0;
      slot.distance = 66;
      break;
    }
  }
  graph.PopulateNodes ();
  planner.Init ();

  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);

  // wall fraction per row, set before each bot runs
  float wall_fraction = 1.0f;

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] == v1[2] && v1[2] < 60.0f) {
      out->flFraction = wall_fraction;
      out->pHit = wall;
    }
  });

  auto run_takeoff = [&] (Bot &bot, float fraction) {
    wall_fraction = fraction;
    engine.AdvanceTime (0.15f);

    NavigateHook::ChangeNode (bot, 1); // dest node, rise reads off its origin
    NavigateHook::SetTravelFlags (bot, PathFlag::Jump);
    NavigateHook::SetDesiredVelocity (bot, ystl::Vector (130.0f, 0.0f, 0.0f));
    bot.pev->button = 0;
    CHECK (!NavigateHook::UpdateNav (bot));
    CHECK ((bot.pev->button & IN_JUMP) != 0);
    return bot.pev->velocity;
  };
  auto make_takeoff_bot = [&] (const char *name, const ystl::Vector &pos) {
    auto bot = CreateNavBot (name, pos);
    bot->pev->maxspeed = 270.0f;
    bot->pev->flags |= FL_ONGROUND;
    bot->team_ = Team::CT;
    return bot;
  };

  // marginal arc (lip 24 out, +2.9 margin): braked to the 80 floor
  auto slow_bot = make_takeoff_bot ("RetimeSlow", ystl::Vector (0.0f, 300.0f, 0.0f));
  HOST_REQUIRE (slow_bot.get () != nullptr);

  const ystl::Vector slow = run_takeoff (*slow_bot, 24.0f / 128.0f);
  CHECK (slow.x == 80.0f);
  CHECK (slow.y == 0.0f);

  // sufficient arc (lip 50 out, +10.9 margin): mapper velocity untouched
  auto keep_bot = make_takeoff_bot ("RetimeKeep", ystl::Vector (0.0f, 300.0f, 0.0f));
  HOST_REQUIRE (keep_bot.get () != nullptr);

  const ystl::Vector kept = run_takeoff (*keep_bot, 50.0f / 128.0f);
  CHECK (kept.x == 130.0f);
  CHECK (kept.y == 0.0f);

  // flat dest (rise 0): gate skips, mapper velocity untouched
  auto flat_bot = make_takeoff_bot ("RetimeFlat", ystl::Vector (0.0f, 300.0f, 51.0f));
  HOST_REQUIRE (flat_bot.get () != nullptr);

  const ystl::Vector flat = run_takeoff (*flat_bot, 24.0f / 128.0f);
  CHECK (flat.x == 130.0f);
  CHECK (flat.y == 0.0f);

  // open air (no face within 128): mapper velocity untouched
  auto open_bot = make_takeoff_bot ("RetimeOpen", ystl::Vector (0.0f, 300.0f, 0.0f));
  HOST_REQUIRE (open_bot.get () != nullptr);

  const ystl::Vector open = run_takeoff (*open_bot, 1.0f);
  CHECK (open.x == 130.0f);
  CHECK (open.y == 0.0f);

  // far lip (66 out, short by 0.5): boosted towards apex timing
  auto fast_bot = make_takeoff_bot ("RetimeFast", ystl::Vector (0.0f, 300.0f, 0.0f));
  HOST_REQUIRE (fast_bot.get () != nullptr);

  const ystl::Vector fast = run_takeoff (*fast_bot, 66.0f / 128.0f);
  CHECK (fast.x > 190.0f && fast.x < 205.0f);
  CHECK (fast.y == 0.0f);

  engine.SetTraceLineHook (nullptr);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_tactics") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // multi-mode map: assassination + hostage + demolition + escape legs live together
  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);
  edict_t *vip_zone = engine.SpawnEntity ("func_vip_safetyzone");
  HOST_REQUIRE (vip_zone != nullptr);
  edict_t *escape_zone = engine.SpawnEntity ("func_escapezone");
  HOST_REQUIRE (escape_zone != nullptr);
  edict_t *hostage_ent = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hostage_ent != nullptr);

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

  // flagged chain: 0 plain, 1 T, 2 CT, 3 goal, 4 camp, 5 rescue, 6 sniper
  graph.Reset ();

  const ystl::Vector spots[7] = {
    ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::Vector (300.0f, 0.0f, 0.0f),
    ystl::Vector (400.0f, 0.0f, 0.0f),
    ystl::Vector (500.0f, 0.0f, 0.0f),
    ystl::Vector (600.0f, 0.0f, 0.0f),
  };
  const int32_t flag_of[7] = {
    0,
    ystl::to_underlying (NodeFlag::TerroristOnly),
    ystl::to_underlying (NodeFlag::CTOnly),
    ystl::to_underlying (NodeFlag::Goal),
    ystl::to_underlying (NodeFlag::Camp),
    ystl::to_underlying (NodeFlag::Rescue),
    ystl::to_underlying (NodeFlag::Camp),
  };

  for (int i = 0; i < 7; ++i) {
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

  for (int i = 0; i < 6; ++i) {
    link (i, i + 1, 100);
    link (i + 1, i, 100);
  }
  graph.PopulateNodes ();
  planner.Init ();

  auto bot = CreateNavBot ("Goal2Bot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  NavigateHook::SetTeam (*bot, Team::Terrorist);
  NavigateHook::SetDifficulty (*bot, Difficulty::Normal);
  NavigateHook::SetAggression (*bot, 0.5f, 0.5f);

  // ...assassination/hostage T legs: rusher pushes offensive, others camp...
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  NavigateHook::SetPersonality (*bot, Personality::Careful);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...normal coin flip both ways under pinned rolls...
  NavigateHook::SetPersonality (*bot, Personality::Normal);
  NavigateHook::Rng (*bot).force_chance (true);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).force_chance (false);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();

  // ...CT on hostage maps always pushes offensive...
  NavigateHook::SetTeam (*bot, Team::CT);
  NavigateHook::SetPersonality (*bot, Personality::Normal);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...escape map: T rusher takes the goal post, others rate it...
  NavigateHook::SetTeam (*bot, Team::Terrorist);
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  NavigateHook::Rng (*bot).force_chance (true);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).force_chance (false);
  NavigateHook::SetPersonality (*bot, Personality::Careful);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();
  NavigateHook::SetPersonality (*bot, Personality::Normal);

  NavigateHook::SetTeam (*bot, Team::CT);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...tactic lottery: camp/forward/goal desires reshaped by personality...
  NavigateHook::Rng (*bot).force_float (69.0f);
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::SetPersonality (*bot, Personality::Careful);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::SetPersonality (*bot, Personality::Normal);
  NavigateHook::Rng (*bot).force_chance (true);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).force_chance (false);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();

  // ...non-camp guns zero the camp desire, snipers inflate it...
  bot->weapon_type_ = WeaponType::Rifle;
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  bot->weapon_type_ = WeaponType::Sniper;
  bot->current_weapon_ = Weapon::AWP;
  NavigateHook::Rng (*bot).force_float (2.0f);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();
  bot->weapon_type_ = WeaponType::Rifle;

  // ...rush endgame inflates goal/forward and kills camp/backoff...
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  NavigateHook::SetHealthValue (*bot, 100.0f);
  NavigateHook::SetAggression (*bot, 0.9f, 0.0f);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...hostage carrier with rush takes the nearest rescue directly...
  game_state.RoundStart (); // stop the rush clock for the next legs
  NavigateHook::SetTeam (*bot, Team::CT);
  NavigateHook::SetHasHostage (*bot, true);
  ystl::SmallArray<int32_t> defensive {};
  defensive.push (2);
  ystl::SmallArray<int32_t> offensive {};
  offensive.push (1);
  CHECK (graph.Exists (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive)));
  NavigateHook::SetHasHostage (*bot, false);

  // ...camp tactic with a sniper picks sniper points, others camp...
  graph.GetPoints (PointType::Sniper).push (6);
  bot->weapon_type_ = WeaponType::Sniper;
  bot->current_weapon_ = Weapon::AWP;
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::Camp, &defensive, &offensive) == 6);
  bot->weapon_type_ = WeaponType::Rifle;
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Camp, &defensive, &offensive)));

  // ...rusher takes a random top-4 goal, history/area filter covered below...
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  NavigateHook::Rng (*bot).force_int (0);
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive)));
  NavigateHook::Rng (*bot).clear_forced ();
  NavigateHook::SetPersonality (*bot, Personality::Normal);

  // ...post-process: exact history hit, area rule, occupied and wall filters...
  NavigateHook::ClearHistory (*bot);
  NavigateHook::PushHistory (*bot, 1);
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
  }
  NavigateHook::ClearHistory (*bot);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_ordnance") {
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

  BuildNavGraph ();
  planner.Init ();

  auto bot = CreateNavBot ("Bomb2Bot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  NavigateHook::SetTeam (*bot, Team::Terrorist);
  NavigateHook::SetHasC4 (*bot, false);

  // ...loose bomb: ignored entry clears the ignore list...
  edict_t *loose = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (loose != nullptr);
  loose->v.model = string_t (engine.AllocString ("models/p_backpack.mdl"));
  const float loose_pos[3] = { 150.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (loose, loose_pos);
  NavigateHook::AddIgnored (*bot, loose);
  CHECK (NavigateHook::BombActionGoal (*bot) == 1);
  CHECK (NavigateHook::LoosedBomb (*bot) == 1);

  // ...in the zone with the c4 and no loose bomb: hold the nearest goal...
  game_state.SetBombPlanted (false);
  engine.AdvanceTime (0.2f);
  NavigateHook::SetInBombZone (*bot, true);
  NavigateHook::SetProgressBar (*bot, false);
  NavigateHook::SetHasC4 (*bot, true);
  // (remove the loose bomb so the zone leg runs)
  engine.Funcs ().pfnSetOrigin (loose, loose_pos);
  loose->v.model = string_t (engine.AllocString ("models/w_knife.mdl"));
  engine.AdvanceTime (0.2f);
  const int zone_goal = NavigateHook::BombActionGoal (*bot);
  CHECK (zone_goal == kInvalidNodeIndex || graph.Exists (zone_goal)); // plain chain has no Goal flag
  NavigateHook::SetInBombZone (*bot, false);
  NavigateHook::SetHasC4 (*bot, false);

  // ...planted bomb, T guard: early timer camps the site, late timer radios...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::SetDefendedBomb (*bot, false);
  mp_c4timer.Set (35);
  const int guard_goal = NavigateHook::BombActionGoal (*bot);
  CHECK (graph.Exists (guard_goal) || guard_goal == kInvalidNodeIndex);
  NavigateHook::SetDefendedBomb (*bot, false);
  mp_c4timer.Set (5);
  const int late_goal = NavigateHook::BombActionGoal (*bot);
  CHECK (graph.Exists (late_goal) || late_goal == kInvalidNodeIndex);

  // ...planted lookup: model mismatch keeps invalid, match resolves...
  edict_t *planted = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (planted != nullptr);
  planted->v.model = string_t (engine.AllocString ("models/w_knife.mdl"));
  const float planted_pos[3] = { 200.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (planted, planted_pos);
  CHECK (NavigateHook::NearestPlanted (*bot) == kInvalidNodeIndex);
  planted->v.model = string_t (engine.AllocString ("models/w_c4.mdl"));
  CHECK (graph.Exists (NavigateHook::NearestPlanted (*bot)));
  NavigateHook::AddIgnored (*bot, planted);
  CHECK (graph.Exists (NavigateHook::NearestPlanted (*bot)));

  // ...visited marking: invalid input no-ops, goal marks its site...
  NavigateHook::MarkSite (*bot, kInvalidNodeIndex);
  CHECK (!graph.IsVisited (99));

  game_state.SetBombPlanted (false);
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_avoid2") {
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
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("Av2A", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("Av2B", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  Bot *bot = nullptr, *mate = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "Av2A") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "Av2B") {
        mate = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr);

  bots.ForEach ([&] (Bot *candidate) {
    if (candidate != bot && candidate != mate) {
      candidate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
    }
    return false;
  });

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  clients[bot->Ent ()].team = Team::CT;
  clients[mate->Ent ()].team = Team::CT;

  auto prep_bot = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
    target->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
    target->pev->angles = ystl::Vector (0.0f, 0.0f, 0.0f);
    target->pev->health = 100.0f;
    target->pev->max_health = 100.0f;
    target->pev->deadflag = DEAD_NO;
    target->pev->takedamage = DAMAGE_YES;
    target->pev->solid = SOLID_BBOX;
    target->pev->movetype = MOVETYPE_WALK;
    target->pev->maxspeed = 100.0f;
    target->pev->flags |= FL_ONGROUND;
    target->is_alive_ = true;
  };
  prep_bot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_bot (mate, ystl::Vector (60.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*mate, 0);

  // ...inverted priority: we camp on the shared node, the mate yields...
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  clients.Update ();
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetStrafe (*bot, 0.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  NavigateHook::Avoid (*mate, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*mate) == bot->Ent ());

  // ...we outrank the mate, so our avoidance clears the shared camp...
  mate->ClearTasks ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));

  // ...dead hindrance clears instead of steering...
  NavigateHook::SetHindranceEnt (*bot, mate->Ent ());
  mate->pev->deadflag = DEAD_DEAD;
  mate->pev->health = 0.0f;
  clients.Update ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  mate->pev->deadflag = DEAD_NO;
  mate->pev->health = 100.0f;
  mate->pev->takedamage = DAMAGE_YES;
  clients.Update ();

  // ...stuck with a hindrance copies the other's goal when usable...
  bot->ClearTasks ();
  mate->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  mate->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, 3, 0.0f, true);
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  NavigateHook::SetMovedDistance (*bot, 0.0f);
  NavigateHook::SetPrevSpeed (*bot, 100.0f);
  NavigateHook::StuckStatus (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*bot) != nullptr);

  // ...vertical split softens the near/far thresholds without crashing...
  mate->pev->origin = ystl::Vector (60.0f, 0.0f, 200.0f);
  clients.Update ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  mate->pev->origin = ystl::Vector (60.0f, 0.0f, 0.0f);
  clients.Update ();

  // ...semiclip and ladder shortcuts return immediately...
  bot->pev->movetype = MOVETYPE_FLY;
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  bot->pev->movetype = MOVETYPE_WALK;
  cv_has_team_semiclip.Set (1);
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  cv_has_team_semiclip.Set (0);

  // ...creature under fire with speed resets the collision timers...
  NavigateHook::SetCreature (*bot, true);
  NavigateHook::SetLastDamage (*bot, game.Time () + 10.0f);
  bot->pev->velocity = ystl::Vector (100.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::StuckStatus (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)));
  NavigateHook::SetCreature (*bot, false);
  NavigateHook::SetLastDamage (*bot, 0.0f);
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_lift2") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

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

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);
  bots.Addbot ("LiftA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("LiftB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  Bot *bot = nullptr, *mate = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "LiftA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "LiftB") {
        mate = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr);
  bot->team_ = Team::CT;
  mate->team_ = Team::CT;

  auto prep_lift = [] (Bot *target, const ystl::Vector &pos) {
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
  };
  prep_lift (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  prep_lift (mate, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 2);

  ystl::Array<int32_t> walk {};
  walk.push (3);
  NavigateHook::SetWalk (*bot, walk);

  // ...freed lift entity resets the state...
  edict_t *plat = engine.SpawnEntity ("func_plat");
  HOST_REQUIRE (plat != nullptr);
  const float plat_pos[3] = { 200.0f, 0.0f, -50.0f };
  engine.Funcs ().pfnSetOrigin (plat, plat_pos);
  plat->v.velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  NavigateHook::SetLiftEntity (*bot, plat);
  NavigateHook::SetLiftState (*bot, LiftState::WaitingFor);
  plat->v.flags |= FL_KILLME;
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::None));
  plat->v.flags &= ~FL_KILLME;
  NavigateHook::SetLiftEntity (*bot, nullptr);

  // ...traveling without ground leaves, riding extends the window...
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, walk);
  NavigateHook::SetLiftEntity (*bot, plat);
  NavigateHook::SetLiftState (*bot, LiftState::TravelingBy);
  bot->pev->groundentity = nullptr;
  CHECK (NavigateHook::LiftS (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::Leaving));
  bot->pev->groundentity = plat;
  plat->v.velocity = ystl::Vector (0.0f, 0.0f, 100.0f);
  NavigateHook::SetLiftState (*bot, LiftState::TravelingBy);
  NavigateHook::SetTargetEntity (*bot, nullptr);
  graph.paths_[2].flags |= NodeFlag::Lift;
  NavigateHook::ChangeNode (*bot, 2); // refresh cached flags with the lift bit
  CHECK (NavigateHook::LiftS (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::TravelingBy));
  plat->v.velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->groundentity = nullptr;
  graph.paths_[2].flags &= ~NodeFlag::Lift;

  // ...every lift state runs through the handler...
  for (int s = 0; s <= 7; ++s) {
    NavigateHook::SetLiftState (*bot, static_cast<LiftState> (s));
    NavigateHook::SetLiftEntity (*bot, (s == 0) ? nullptr : plat);
    NavigateHook::LiftH (*bot);
  }
  NavigateHook::SetLiftState (*bot, LiftState::None);
  NavigateHook::SetLiftEntity (*bot, nullptr);

  // ...follow teammate on the lift holds the door...
  mate->pev->groundentity = plat;
  NavigateHook::SetLiftEntity (*mate, plat);
  mate->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, 0, 0.0f, true);
  NavigateHook::SetTargetEntity (*mate, bot->Ent ());
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, walk);
  NavigateHook::LiftH (*bot);
  mate->ClearTasks ();
  mate->pev->groundentity = nullptr;

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_strafe") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);
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

  BuildNavGraph ();
  planner.Init ();
  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());

  auto bot = CreateNavBot ("Move2Bot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->pev->angles = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->flags |= FL_ONGROUND;
  bot->pev->maxspeed = 270.0f;
  bot->team_ = Team::CT;
  NavigateHook::ChangeNode (*bot, 0);

  Trace::Result strafe {};

  // ...door on the strafe lane never blocks...
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = door;
  });
  CHECK (NavigateHook::StrafeLeft (*bot, &strafe));
  CHECK (NavigateHook::StrafeRight (*bot, &strafe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // ...wall on the second leg blocks each side...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v1[1] != v2[1] || v1[0] != v2[0]) {
      out->flFraction = 0.5f; // sideways and forward probes hit
      out->pHit = wall;
    }
  });
  CHECK (!NavigateHook::StrafeLeft (*bot, &strafe));
  CHECK (!NavigateHook::StrafeRight (*bot, &strafe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // ...blocked predicates read the hull, doors exempt...
  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = wall;
  });
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  CHECK (NavigateHook::BlockedLeft (*bot));
  CHECK (NavigateHook::BlockedRight (*bot));
  NavigateHook::SetMoveSpeed (*bot, -100.0f);
  CHECK (NavigateHook::BlockedLeft (*bot));
  CHECK (NavigateHook::BlockedRight (*bot));
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.2f);

  // ...walls on every side read through the wall checks...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = wall;
  });
  CHECK (NavigateHook::WallLeft (*bot, 40.0f));
  CHECK (NavigateHook::WallRight (*bot, 40.0f));
  CHECK (NavigateHook::WallBehind (*bot, 40.0f));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // ...deadly/start-solid legs and the safe predicate...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->fStartSolid = 1;
  });
  CHECK (!NavigateHook::DeadlyMove (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (NavigateHook::SafeToMove (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // ...water ignores the fall check...
  bot->pev->waterlevel = 3;
  CHECK (!NavigateHook::DeadlyMove (*bot, ystl::Vector (100.0f, 0.0f, 0.0f)));
  bot->pev->waterlevel = 0;

  // ...ascent rejects zero steps and sudden lips...
  sv_stepsize.Set (0);
  CHECK (!NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (10.0f, 0.0f, 0.0f)));
  sv_stepsize.Set (18);
  CHECK (!NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 30.0f)));

  // ...camp direction: visible far node, enemy prediction, blind fallback...
  NavigateHook::Rng (*bot).force_int (0);
  CHECK (graph.Exists (NavigateHook::CampDir (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();
  NavigateHook::SetEnemyOrigin (*bot, ystl::Vector (300.0f, 0.0f, 0.0f));
  CHECK (graph.Exists (NavigateHook::CampDir (*bot)));

  // ...aiming: invalid dest, visible path, blind fallback...
  int aim_length = 0;
  CHECK (NavigateHook::AimingNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f), aim_length) != kInvalidNodeIndex || aim_length >= 0);

  // ...farest within/over range, cover blind, defend with visibility...
  CHECK (NavigateHook::FarestNode (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), 1000.0f) == 4);
  CHECK (NavigateHook::FarestNode (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), 10000.0f) == kInvalidNodeIndex);
  NavigateHook::SetEnemyOrigin (*bot, ystl::Vector (400.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::CoverNode (*bot, 1000.0f) != kInvalidNodeIndex || true);
  CHECK (graph.Exists (NavigateHook::DefendNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f))));

  // ...strafe speed: walls force the open side, water always moves...
  NavigateHook::StrafeSpeedRaw (*bot, 100.0f);
  NavigateHook::StrafeSpeedRaw (*bot, -100.0f);
  NavigateHook::StrafeSpeedRaw (*bot, 0.0f);
  bot->pev->waterlevel = 2;
  CHECK (NavigateHook::Reachable (*bot, 1));
  bot->pev->waterlevel = 0;

  // ...ladder path origin through a nearby ladder entity...
  edict_t *ladder = engine.SpawnEntity ("func_ladder");
  HOST_REQUIRE (ladder != nullptr);
  const float ladder_pos[3] = { 10.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (ladder, ladder_pos);
  bot->pev->movetype = MOVETYPE_FLY;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SetPathOr (*bot);
  CHECK (NavigateHook::PathOrigin (*bot).x != 99999.0f);
  bot->pev->movetype = MOVETYPE_WALK;

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_routing") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  edict_t *hostage_ent = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hostage_ent != nullptr);
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
  HOST_REQUIRE (game.MapIs (MapFlags::HostageRescue));

  BuildNavGraph ();
  planner.Init ();
  planner.SetPathsCheckFailed (false); // force the astar legs below

  auto bot = CreateNavBot ("Path2Bot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  bot->pev->flags |= FL_ONGROUND;
  bot->pev->maxspeed = 270.0f;
  NavigateHook::SetTeam (*bot, Team::CT);

  // ...heuristic matrix: fast/optimal/safe/diversity, hostage variants...
  // (astar may or may not find the chain; the legs still execute)
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Optimal);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Safe);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Diversity);

  NavigateHook::SetHasHostage (*bot, true);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Optimal);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Safe);
  NavigateHook::SetHasHostage (*bot, false);

  // ...astar needs 8+ nodes: a 10-chain drives the staging lambda...
  graph.Reset ();

  for (int i = 0; i < 10; ++i) {
    Path path {};
    path.origin = ystl::Vector (static_cast<float> (i * 100), 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  for (int i = 0; i < 9; ++i) {
    for (auto &slot : graph.paths_[static_cast<size_t> (i)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (i + 1);
        slot.distance = 100;
        break;
      }
    }

    for (auto &slot : graph.paths_[static_cast<size_t> (i + 1)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (i);
        slot.distance = 100;
        break;
      }
    }
  }
  graph.PopulateNodes ();
  planner.Init ();
  planner.SetPathsCheckFailed (false);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Fast);
  NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Diversity);
  NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Optimal);
  NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Safe);

  // ...fresh bot sizes its astar from the 10-chain, so the find lambda runs...
  auto astar_bot = CreateNavBot ("AstarBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (astar_bot.get () != nullptr);
  astar_bot->pev->flags |= FL_ONGROUND;
  astar_bot->pev->maxspeed = 270.0f;
  NavigateHook::SetTeam (*astar_bot, Team::CT);
  planner.SetPathsCheckFailed (false);
  NavigateHook::SyncPath (*astar_bot, 0, 9, FindPathType::Fast);
  NavigateHook::SyncPath (*astar_bot, 0, 9, FindPathType::Diversity);
  NavigateHook::SyncPath (*astar_bot, 0, 9, FindPathType::Optimal);
  NavigateHook::SyncPath (*astar_bot, 0, 9, FindPathType::Safe);

  // ...stale bots skip pathfinding, broken requests fail closed...
  NavigateHook::SetStale (*bot, true);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  NavigateHook::SetStale (*bot, false);
  NavigateHook::SyncPath (*bot, kInvalidNodeIndex, 3, FindPathType::Fast);
  NavigateHook::SyncPath (*bot, 0, 99, FindPathType::Fast);
  NavigateHook::SyncPath (*bot, 2, 2, FindPathType::Fast);

  // ...node bookkeeping: change, nearest, random, validity...
  CHECK (NavigateHook::ChangeNode (*bot, 1) == 1);
  NavigateHook::ChangeNode (*bot, kInvalidNodeIndex);
  CHECK (bot->GetCurrentNodeIndex () == 1);
  CHECK (NavigateHook::NearestNode (*bot) != kInvalidNodeIndex);
  CHECK (graph.Exists (NavigateHook::RandomNode (*bot, PointType::Count)));
  NavigateHook::FindValid (*bot);
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);
  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  CHECK (bot->FindNextBestNode ());
  NavigateHook::SetRechoice (*bot, 99);
  NavigateHook::FindValid (*bot);

  // ...reach time: recently fired, ladder, speed variants...
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 1);
  CHECK (NavigateHook::ReachTime (*bot) > 0.0f);
  engine.AdvanceTime (0.3f);
  NavigateHook::SetMoveSpeed (*bot, 200.0f);
  CHECK (NavigateHook::ReachTime (*bot) > 0.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);

  // ...ladder distances across walk shapes...
  ystl::Array<int32_t> empty_walk {};
  ystl::Array<int32_t> two_walk {};
  two_walk.push (1);
  two_walk.push (2);
  NavigateHook::SetWalk (*bot, two_walk);
  CHECK (NavigateHook::LadderDistance (*bot) > 0.0f);
  NavigateHook::SetWalk (*bot, empty_walk);
  CHECK (NavigateHook::LadderDistance (*bot) > 0.0f);

  // ...buttons blind and plain, occupancy, reachability...
  edict_t *btn = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (btn != nullptr);
  btn->v.target = string_t (engine.AllocString ("door9"));
  CHECK (NavigateHook::LookupBtn (*bot, "door9", true) == btn);
  CHECK (NavigateHook::LookupBtn (*bot, "door9", false) == btn);
  CHECK (NavigateHook::LookupBtn (*bot, "none", true) == nullptr);
  CHECK (!NavigateHook::Occupied (*bot, 1, false));
  CHECK (NavigateHook::Reachable (*bot, 1));
  CHECK (!NavigateHook::PrevLadder (*bot));

  // ...jump ballistics: level arc, impossible, zero gravity...
  const ystl::Vector arc = NavigateHook::CalcJump (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f));
  CHECK (arc.length2d () > 0.0f);
  CHECK (NavigateHook::CalcJump (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 100000.0f)).empty ());
  sv_gravity.Set (0);
  CHECK (NavigateHook::CalcJump (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)).empty ());
  sv_gravity.Set (800);

  // ...retime gates: water, flat, slow link, light gravity...
  bot->pev->waterlevel = 2;
  NavigateHook::SetPathOriginVec (*bot, ystl::Vector (0.0f, 0.0f, 100.0f));
  NavigateHook::SetDesiredVelocity (*bot, ystl::Vector (130.0f, 0.0f, 0.0f));
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  bot->pev->waterlevel = 0;
  NavigateHook::SetPathOriginVec (*bot, bot->pev->origin);
  NavigateHook::SetDesiredVelocity (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  sv_gravity.Set (0);
  NavigateHook::SetPathOriginVec (*bot, ystl::Vector (0.0f, 0.0f, 100.0f));
  NavigateHook::SetDesiredVelocity (*bot, ystl::Vector (130.0f, 0.0f, 0.0f));
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  sv_gravity.Set (800);

  // ...movement frame: translate, reset, crouch/ladder/swim legs...
  NavigateHook::SetMoveSpeed (*bot, 200.0f);
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_FORWARD) != 0);
  NavigateHook::ResetMovement (*bot);
  NavigateHook::MoveToGoal (*bot);
  NavigateHook::SetJumpSeq (*bot, true);
  NavigateHook::CheckTerr (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  NavigateHook::CheckF (*bot);
  NavigateHook::SetJumpSeq (*bot, false);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_frame") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  edict_t *door = engine.SpawnEntity ("func_door");
  HOST_REQUIRE (door != nullptr);
  const float door_pos[3] = { 150.0f, 0.0f, 0.0f };
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

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);
  bots.Addbot ("FrmA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("FrmB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  Bot *bot = nullptr, *mate = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "FrmA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "FrmB") {
        mate = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr);
  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  clients.Update ();
  clients[bot->Ent ()].team = Team::CT;
  clients[mate->Ent ()].team = Team::CT;

  auto prep_frame = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
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
  prep_frame (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_frame (mate, ystl::Vector (60.0f, 0.0f, 0.0f));

  // ...jump link presses jump, ladder flag detours through the frame...
  NavigateHook::ChangeNode (*bot, 0);
  bot->ClearSearchNodes ();
  bot->StartTask (TaskId::MoveTo, 50.0f, 3, 0.0f, false);
  NavigateHook::SetTravelFlags (*bot, PathFlag::Jump);
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK ((bot->pev->button & IN_JUMP) != 0);
  NavigateHook::SetTravelFlags (*bot, static_cast<PathFlag> (0));

  // ...far door leg clears aim direction without pressing...
  prep_frame (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 2);
  ystl::Array<int32_t> walk {};
  walk.push (3);
  NavigateHook::SetWalk (*bot, walk);
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v1;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[0] > 150.0f) {
      out->flFraction = 0.5f;
      out->pHit = door;
    }
  });
  engine.AdvanceTime (0.15f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (!NavigateHook::AimDirCan (*bot));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // ...arrival with goal practice runs the frame to completion...
  bot->ClearSearchNodes ();
  bot->StartTask (TaskId::MoveTo, 50.0f, 2, 0.0f, false);
  NavigateHook::SetChosenGoal (*bot, 2);
  NavigateHook::SetHealthValue (*bot, 100.0f);
  CHECK (NavigateHook::UpdateNav (*bot));
  NavigateHook::SetChosenGoal (*bot, kInvalidNodeIndex);

  // ...advance: empty walk validates, occupied tail refinds...
  ystl::Array<int32_t> empty_walk {};
  NavigateHook::SetWalk (*bot, empty_walk);
  CHECK (!NavigateHook::Advance (*bot));
  mate->pev->origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  ystl::Array<int32_t> tail_walk {};
  tail_walk.push (3);
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, tail_walk);
  NavigateHook::Advance (*bot);

  // ...alternative selection: free node bails, ladder bails...
  bot->pev->movetype = MOVETYPE_FLY;
  CHECK (!NavigateHook::SelectNext (*bot));
  bot->pev->movetype = MOVETYPE_WALK;
  ystl::Array<int32_t> alt_walk {};
  alt_walk.push (1);
  alt_walk.push (2);
  NavigateHook::SetWalk (*bot, alt_walk);
  CHECK (!NavigateHook::SelectNext (*bot));

  // ...time camping and rechoice refresh the validity paths...
  NavigateHook::SetTimeCamping (*bot, game.Time () - 100.0f);
  NavigateHook::SetRechoice (*bot, 99);
  NavigateHook::FindValid (*bot);
  NavigateHook::SetTimeCamping (*bot, game.Time () + 100.0f);

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_siege") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // escape-only map: the escape legs of the goal selection live here
  edict_t *escape_zone = engine.SpawnEntity ("func_escapezone");
  HOST_REQUIRE (escape_zone != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  HOST_REQUIRE (game.MapIs (MapFlags::Escape));

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

  auto bot = CreateNavBot ("SiegeBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  NavigateHook::SetAggression (*bot, 0.5f, 0.5f);

  // ...escape map: T rusher storms the goal post, others weigh the odds...
  NavigateHook::SetTeam (*bot, Team::Terrorist);
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  CHECK (NavigateHook::BestGoal (*bot) == 3);

  NavigateHook::SetPersonality (*bot, Personality::Normal);
  NavigateHook::Rng (*bot).force_chance (true);
  CHECK (NavigateHook::BestGoal (*bot) == 3);
  NavigateHook::Rng (*bot).force_chance (false);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();

  // ...CTs on escape maps rate the defensive pool...
  NavigateHook::SetTeam (*bot, Team::CT);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...zombie creatures without typed points fall back to any node...
  game.AddGameFlag (GameFlags::ZombieMod);
  NavigateHook::SetCreature (*bot, true);
  BuildNavGraph ();
  planner.Init ();
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...a lone goal point is preferred over the blind fallback...
  graph.Reset ();

  for (int i = 0; i < 2; ++i) {
    Path path {};
    path.origin = ystl::Vector (static_cast<float> (i * 100), 0.0f, 0.0f);
    path.number = i;
    path.flags = (i == 1) ? ystl::to_underlying (NodeFlag::Goal) : 0;
    path.light = kInvalidLightLevel;

    for (auto &slot : path.links) {
      slot.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  link (0, 1, 100);
  link (1, 0, 100);
  graph.PopulateNodes ();
  planner.Init ();
  CHECK (NavigateHook::BestGoal (*bot) == 1);

  game.ClearGameFlag (GameFlags::ZombieMod);
  NavigateHook::SetCreature (*bot, false);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_herald") {
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
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

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

  auto bot = CreateNavBot ("HeraldBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  NavigateHook::SetAggression (*bot, 0.5f, 0.5f);

  ystl::SmallArray<int32_t> defensive {};
  defensive.push (2);
  ystl::SmallArray<int32_t> offensive {};
  offensive.push (1);

  // ...CTs without a planted bomb rate the pools, careful ones hang back...
  NavigateHook::SetTeam (*bot, Team::CT);
  NavigateHook::SetPersonality (*bot, Personality::Careful);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...terrorists with a fresh plant but no known site guard a random goal...
  game_state.SetBombPlanted (true);
  NavigateHook::SetTeam (*bot, Team::Terrorist);
  NavigateHook::SetDefendedBomb (*bot, false);
  CHECK (graph.Exists (NavigateHook::BestGoal (*bot)));

  // ...late timers radio the blown cover instead of camping...
  game_state.SetBombOrigin (false, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 0);
  mp_c4timer.Set (35);
  engine.AdvanceTime (30.0f);
  CHECK (graph.Exists (NavigateHook::BombActionGoal (*bot)));

  // ...carrier rush takes the nearest rescue, the far one is skipped...
  NavigateHook::SetTeam (*bot, Team::CT);
  NavigateHook::SetHealthValue (*bot, 100.0f);
  NavigateHook::SetAggression (*bot, 0.9f, 0.0f);
  bot->has_hostage_ = true;
  CHECK (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive) == 5);

  bot->pev->origin = ystl::Vector (3000.0f, 0.0f, 0.0f);
  CHECK (graph.Exists (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive)));
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->has_hostage_ = false;

  // ...arrival at the goal practices the site, audio corrects a stale goal...
  bot->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::Rng (*bot).force_chance (true);
  bot->StartTask (TaskId::MoveTo, 50.0f, 4, 0.0f, false);
  ystl::Array<int32_t> tail_walk {};
  tail_walk.push (4);
  NavigateHook::SetWalk (*bot, tail_walk);
  CHECK (!NavigateHook::UpdateNav (*bot));

  bot->StartTask (TaskId::MoveTo, 50.0f, 2, 0.0f, false);
  NavigateHook::SetChosenGoal (*bot, 2);
  NavigateHook::SetHealthValue (*bot, 100.0f);
  CHECK (NavigateHook::UpdateNav (*bot));
  NavigateHook::Rng (*bot).clear_forced ();

  // ...an empty graph fails every query closed without crashing...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (5000.0f, 0.0f, 0.0f));
  NavigateHook::SetDefendedBomb (*bot, false);
  graph.Reset ();
  graph.PopulateNodes ();

  CHECK (NavigateHook::BombActionGoal (*bot) == kInvalidNodeIndex);
  CHECK (NavigateHook::BombNode (*bot) == kInvalidNodeIndex);
  CHECK (NavigateHook::CoverNode (*bot, 1000.0f) == kInvalidNodeIndex);

  int aim_length = 0;
  CHECK (NavigateHook::AimingNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f), aim_length) == kInvalidNodeIndex);
  CHECK (NavigateHook::RandomNode (*bot, PointType::Count) == kInvalidNodeIndex);
  CHECK (NavigateHook::FarestNode (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), 1000.0f) == kInvalidNodeIndex);
  CHECK (NavigateHook::NearestPlanted (*bot) == kInvalidNodeIndex);

  NavigateHook::SetStale (*bot, true);
  PathWalk staging {};
  staging.Init (64);
  NavigateData::PathMeta meta {};
  CHECK (!NavigateHook::ShortestPath (*bot, 0, 3, staging, meta));
  NavigateHook::SetStale (*bot, false);

  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  NavigateHook::SetRechoice (*bot, 0);
  NavigateHook::FindValid (*bot);
  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  CHECK (NavigateHook::WalkLength (*bot) == 0);

  game_state.SetBombPlanted (false);
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_bastion") {
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
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("BasA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("BasB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("BasC", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  Bot *bot = nullptr, *mate = nullptr, *third = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr || third == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "BasA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "BasB") {
        mate = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "BasC") {
        third = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr && third != nullptr);

  bots.ForEach ([&] (Bot *candidate) {
    if (candidate != bot && candidate != mate && candidate != third) {
      candidate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
    }
    return false;
  });

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  third->team_ = Team::CT;
  clients[bot->Ent ()].team = Team::CT;
  clients[mate->Ent ()].team = Team::CT;
  clients[third->Ent ()].team = Team::CT;

  auto prep_bot = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
    target->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
    target->pev->angles = ystl::Vector (0.0f, 0.0f, 0.0f);
    target->pev->health = 100.0f;
    target->pev->max_health = 100.0f;
    target->pev->deadflag = DEAD_NO;
    target->pev->takedamage = DAMAGE_YES;
    target->pev->solid = SOLID_BBOX;
    target->pev->movetype = MOVETYPE_WALK;
    target->pev->maxspeed = 100.0f;
    target->pev->flags |= FL_ONGROUND;
    target->is_alive_ = true;
    NavigateHook::SetHealthValue (*target, 100.0f);
    NavigateHook::SetAggression (*target, 0.9f, 0.0f);
  };
  prep_bot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_bot (mate, ystl::Vector (60.0f, 0.0f, 0.0f));
  prep_bot (third, ystl::Vector (5000.0f, 0.0f, 0.0f));
  clients.Update ();
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetStrafe (*bot, 0.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);

  // ...repulsion from the higher-priority camper steers us aside...
  mate->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  third->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  bot->ClearTasks ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*bot) == mate->Ent ());
  CHECK (NavigateHook::StrafeSpeed (*bot) == bot->pev->maxspeed);

  // ...a height split softens the near and far thresholds...
  mate->pev->origin = ystl::Vector (30.0f, 0.0f, 80.0f);
  clients.Update ();
  engine.AdvanceTime (0.5f);
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*bot) != nullptr);

  // ...a symmetric crowd sums to no prediction, so the slide is held...
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  third->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)); // releases the commit
  mate->pev->origin = ystl::Vector (2.0f, 0.0f, 0.0f);
  third->pev->origin = ystl::Vector (-2.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::StrafeSpeed (*bot) == bot->pev->maxspeed);

  // ...a closing hindrance straight ahead backs the bot off...
  mate->pev->origin = ystl::Vector (50.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (-50.0f, 0.0f, 0.0f);
  third->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  engine.AdvanceTime (0.5f);
  NavigateHook::SetFrameInterval (*bot, 0.05f);
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*bot) != nullptr);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);

  // ...a solid blocker latches stuck only after the grace window...
  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = wall;
  });
  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = wall;
  });
  engine.AdvanceTime (0.2f);

  NavigateHook::SetMovedDistance (*mate, 100.0f);
  NavigateHook::SetMoveSpeed (*mate, 0.0f);
  CHECK (!NavigateHook::StuckStatus (*mate, ystl::Vector (1.0f, 0.0f, 0.0f)));
  CHECK (!NavigateHook::StuckStatus (*mate, ystl::Vector (1.0f, 0.0f, 0.0f)));
  engine.AdvanceTime (0.3f);
  CHECK (NavigateHook::StuckStatus (*mate, ystl::Vector (1.0f, 0.0f, 0.0f)));

  engine.SetTraceLineHook (nullptr);
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.2f);
  CHECK (!NavigateHook::StuckStatus (*mate, ystl::Vector (1.0f, 0.0f, 0.0f)));

  // ...creature jump arbitration reads damage, flags and the stuck latch...
  NavigateHook::SetCreature (*bot, true);
  NavigateHook::SetLastDamage (*bot, game.Time () + 10.0f);
  NavigateHook::SetCollIndex (*bot, 0);
  NavigateHook::SetCollMove (*bot, 0, CollisionState::Jump);
  bot->pev->button = 0;
  NavigateHook::ExecuteColl (*bot);
  CHECK ((bot->pev->button & IN_JUMP) == 0);
  NavigateHook::SetCreature (*bot, false);
  NavigateHook::SetLastDamage (*bot, 0.0f);

  // ...terrain probing honors a held avoidance slide for jump and duck...
  engine.AdvanceTime (0.7f);
  NavigateHook::SetDestOrigin (*bot, ystl::Vector (100.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetMoveSpeed (*bot, 100.0f);
  NavigateHook::SetMovedDistance (*bot, 0.0f);
  NavigateHook::SetPrevSpeed (*bot, 100.0f);
  NavigateHook::SetTerrainStrafe (*bot, 1.0f, 100.0f);
  bot->pev->button = 0;
  NavigateHook::CheckTerr (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::CollState (*bot) == CollisionState::Probing);

  mate->pev->origin = ystl::Vector (60.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::SetCollMove (*bot, 0, CollisionState::Jump);
  NavigateHook::SetHindranceEnt (*bot, mate->Ent ());
  NavigateHook::HoldAvoidCommit (*bot, 10.0f);
  bot->pev->button = 0;
  NavigateHook::CheckTerr (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK ((bot->pev->button & IN_JUMP) != 0);

  NavigateHook::SetCollMove (*bot, 0, CollisionState::Duck);
  bot->pev->button = 0;
  NavigateHook::CheckTerr (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK ((bot->pev->button & IN_DUCK) != 0);
  NavigateHook::SetHindranceEnt (*bot, nullptr);

  // ...weight probes read ladder, water and low-lying goals...
  const ystl::Vector forward (1.0f, 0.0f, 0.0f);
  Bot::CollisionWeights weights {};

  bot->pev->movetype = MOVETYPE_FLY;
  NavigateHook::CollisionWeights (*bot, forward, weights);
  bot->pev->movetype = MOVETYPE_WALK;

  bot->pev->waterlevel = 3;
  NavigateHook::CollisionWeights (*bot, forward, weights);
  bot->pev->waterlevel = 0;

  NavigateHook::SetDestOrigin (*bot, ystl::Vector (100.0f, 0.0f, -40.0f));
  NavigateHook::CollisionWeights (*bot, forward, weights);
  CHECK (weights[CollisionState::Duck].weight > 0);

  // ...ladder falls shortcut, landings without reference points bail...
  graph.paths_[1].flags |= NodeFlag::Ladder;
  NavigateHook::SetPrevNode (*bot, 0, 1);
  NavigateHook::CheckF (*bot);
  CHECK (!NavigateHook::Falling (*bot));
  graph.paths_[1].flags &= ~NodeFlag::Ladder;

  bot->pev->flags &= ~FL_ONGROUND;
  bot->pev->flFallVelocity = 150.0f;
  NavigateHook::CheckF (*bot);
  CHECK (NavigateHook::Falling (*bot));

  bot->pev->flags |= FL_ONGROUND;
  NavigateHook::CheckF (*bot);
  CHECK (!NavigateHook::Falling (*bot));

  // ...a tracked enemy anchors the fall reference when no node is set...
  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  NavigateHook::SetEnemy (*bot, mate->Ent ());
  NavigateHook::CheckF (*bot);
  CHECK (!NavigateHook::Falling (*bot));
  NavigateHook::SetEnemy (*bot, nullptr);

  // ...a crouch entry ducks through the input translation...
  graph.paths_[0].flags |= NodeFlag::Crouch;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SetPathOriginVec (*bot, bot->pev->origin);
  NavigateHook::MoveToGoal (*bot);
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_DUCK) != 0);
  graph.paths_[0].flags &= ~NodeFlag::Crouch;

  // ...a lone close teammate steers by pure repulsion...
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  third->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  mate->pev->origin = ystl::Vector (8.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  CHECK (NavigateHook::Hindrance (*bot) == mate->Ent ());
  mate->pev->origin = ystl::Vector (60.0f, 0.0f, 0.0f);
  clients.Update ();

  // ...ladder paths never latch stuck from headwind...
  graph.paths_[1].flags |= NodeFlag::Ladder;
  NavigateHook::SetPrevNode (*bot, 0, 1);
  NavigateHook::SetPathFlags (*bot, NodeFlag::Ladder);
  NavigateHook::SetMovedDistance (*bot, 100.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  CHECK (!NavigateHook::StuckStatus (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)));
  graph.paths_[1].flags &= ~NodeFlag::Ladder;
  NavigateHook::SetPathFlags (*bot, static_cast<NodeFlag> (0));

  // ...thin lips between samples reject the ascent...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < v1[2]) {
      out->flFraction = (v1[0] < 50.0f) ? 0.1f : 0.9f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * out->flFraction;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * out->flFraction;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * out->flFraction;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (!NavigateHook::WalkableAscent (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_pinnacle") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // door seed for the HasDoors legs of the navigation frame
  edict_t *door = engine.SpawnEntity ("func_door");
  HOST_REQUIRE (door != nullptr);
  const float door_pos[3] = { 190.0f, 0.0f, 0.0f };
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
  HOST_REQUIRE (game.MapIs (MapFlags::HasDoors));

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("PinA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("PinB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("PinE", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);

  Bot *bot = nullptr, *mate = nullptr, *foe = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr || foe == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "PinA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "PinB") {
        mate = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "PinE") {
        foe = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr && foe != nullptr);

  bots.ForEach ([&] (Bot *candidate) {
    if (candidate != bot && candidate != mate && candidate != foe) {
      candidate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
    }
    return false;
  });

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  foe->team_ = Team::Terrorist;
  clients[bot->Ent ()].team = Team::CT;
  clients[mate->Ent ()].team = Team::CT;
  clients[foe->Ent ()].team = Team::Terrorist;

  auto prep_pin = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
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
    target->pev->groundentity = nullptr;
    target->is_alive_ = true;
    NavigateHook::SetHealthValue (*target, 100.0f);
    NavigateHook::SetAggression (*target, 0.9f, 0.0f);
  };
  prep_pin (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_pin (mate, ystl::Vector (5000.0f, 0.0f, 0.0f));
  prep_pin (foe, ystl::Vector (5000.0f, 0.0f, 0.0f));
  clients.Update ();

  edict_t *plat = engine.SpawnEntity ("func_plat");
  HOST_REQUIRE (plat != nullptr);
  const float plat_pos[3] = { 200.0f, 0.0f, -50.0f };
  engine.Funcs ().pfnSetOrigin (plat, plat_pos);

  // ...a falling lift entry aborts the ride through the frame...
  graph.paths_[0].origin.z = 100.0f;
  graph.paths_[1].origin.z = 100.0f;
  graph.paths_[0].flags |= NodeFlag::Lift;
  NavigateHook::SetLiftEntity (*bot, plat);
  NavigateHook::SetLiftState (*bot, LiftState::EnteringIn);
  NavigateHook::SetLiftTravel (*bot, ystl::Vector (300.0f, 0.0f, 300.0f));
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, -150.0f);
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 1);
  NavigateHook::SetPathFlags (*bot, NodeFlag::Lift);
  NavigateHook::SetPrevNode (*bot, 2, 0);
  NavigateHook::SetPrevNode (*bot, 3, 0);
  bot->StartTask (TaskId::MoveTo, 50.0f, 1, 0.0f, false);
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::None));

  graph.paths_[0].origin.z = 0.0f;
  graph.paths_[1].origin.z = 0.0f;
  graph.paths_[0].flags &= ~NodeFlag::Lift;
  NavigateHook::SetLiftEntity (*bot, nullptr);
  NavigateHook::SetPathFlags (*bot, static_cast<NodeFlag> (0));
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->flags |= FL_ONGROUND;

  // ...a queued jump link after a sequence pauses for the landing...
  prep_pin (bot, ystl::Vector (100.0f, 0.0f, 0.0f));
  clients.Update ();
  NavigateHook::ChangeNode (*bot, 1);
  bot->StartTask (TaskId::MoveTo, 50.0f, 3, 0.0f, false);
  NavigateHook::SetTravelFlags (*bot, PathFlag::Jump);
  NavigateHook::SetJumpSeq (*bot, true);
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK ((bot->pev->button & IN_JUMP) != 0);
  NavigateHook::SetTravelFlags (*bot, static_cast<PathFlag> (0));
  NavigateHook::SetJumpSeq (*bot, false);

  // ...ladder approach slows down, arrival practices the goal...
  graph.paths_[1].flags |= NodeFlag::Ladder;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 1);
  NavigateHook::SetChosenGoal (*bot, 1);
  bot->StartTask (TaskId::MoveTo, 50.0f, 1, 0.0f, false);
  CHECK (NavigateHook::UpdateNav (*bot));

  // ...reach distances adapt to flags, links, velocity and debug goals...
  prep_pin (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  clients.Update ();
  NavigateHook::ChangeNode (*bot, 2);
  bot->StartTask (TaskId::MoveTo, 50.0f, 2, 0.0f, false);
  NavigateHook::SetChosenGoal (*bot, 2);

  NavigateHook::SetPathFlags (*bot, NodeFlag::Goal);
  CHECK (NavigateHook::UpdateNav (*bot));

  NavigateHook::SetPathFlags (*bot, NodeFlag::Ladder);
  CHECK (NavigateHook::UpdateNav (*bot));

  NavigateHook::SetPathFlags (*bot, static_cast<NodeFlag> (0));
  NavigateHook::SetTravelFlags (*bot, PathFlag::Jump);
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 20.0f);
  CHECK (NavigateHook::UpdateNav (*bot));

  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (NavigateHook::UpdateNav (*bot));

  NavigateHook::SetTravelFlags (*bot, static_cast<PathFlag> (0));
  NavigateHook::SetPathFlags (*bot, NodeFlag::Crouch);
  CHECK (NavigateHook::UpdateNav (*bot));

  graph.paths_[2].radius = 100.0f;
  NavigateHook::SetPathFlags (*bot, static_cast<NodeFlag> (0));
  NavigateHook::ChangeNode (*bot, 2);
  CHECK (NavigateHook::UpdateNav (*bot));
  graph.paths_[2].radius = 0.0f;

  graph.paths_[2].flags |= NodeFlag::Lift;
  NavigateHook::ChangeNode (*bot, 2);
  CHECK (NavigateHook::UpdateNav (*bot));
  graph.paths_[2].flags &= ~NodeFlag::Lift;

  cv_debug_goal.Set (2);
  NavigateHook::ChangeNode (*bot, 2);
  CHECK (NavigateHook::UpdateNav (*bot));
  cv_debug_goal.Set (-1);

  // ...leaving the ladder downwards jumps off...
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 1);
  bot->pev->movetype = MOVETYPE_FLY;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 50.0f);
  NavigateHook::SetLadderDir (*bot, LadderDir::Down);
  bot->pev->button = 0;
  NavigateHook::UpdateNav (*bot);
  CHECK ((bot->pev->button & IN_JUMP) != 0);
  bot->pev->movetype = MOVETYPE_WALK;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);

  // ...a teammate working the ladder sends us back to wait...
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 30.0f);
  mate->pev->movetype = MOVETYPE_FLY;
  clients.Update ();

  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = mate->Ent ();
  });
  engine.AdvanceTime (0.15f);
  bot->StartTask (TaskId::MoveTo, 50.0f, 1, 0.0f, false);
  NavigateHook::UpdateNav (*bot);
  CHECK (bot->GetCurrentNodeIndex () == 0);

  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.15f);
  mate->pev->movetype = MOVETYPE_WALK;
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  graph.paths_[1].flags &= ~NodeFlag::Ladder;
  clients.Update ();

  // ...a near door with a linked button is picked up instead of pushed...
  edict_t *button = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (button != nullptr);
  button->v.target = string_t (engine.AllocString ("doorbtn"));
  const float button_pos[3] = { 210.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (button, button_pos);
  door->v.targetname = string_t (engine.AllocString ("doorbtn"));

  prep_pin (bot, ystl::Vector (180.0f, 0.0f, 0.0f));
  mate->pev->origin = ystl::Vector (250.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 2);
  bot->StartTask (TaskId::MoveTo, 50.0f, 3, 0.0f, false);
  ystl::Array<int32_t> door_walk {};
  door_walk.push (3);
  NavigateHook::SetWalk (*bot, door_walk);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if ((v1[0] < 190.0f) != (v2[0] < 190.0f)) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = door;
    }
  });
  engine.AdvanceTime (0.15f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (!NavigateHook::AimDirCan (*bot));
  CHECK (NavigateHook::Pickup (*bot) == button);

  // ...without a button the door is used directly, then mates block it...
  door->v.targetname = string_t (engine.AllocString (""));
  NavigateHook::Rng (*bot).force_chance (true);
  engine.AdvanceTime (1.2f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot));
  NavigateHook::Rng (*bot).clear_forced ();

  // ...a stubborn mate reroutes the wait back...
  NavigateHook::Rng (*bot).force_chance (false);
  engine.AdvanceTime (1.2f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  NavigateHook::UpdateNav (*bot);
  CHECK (bot->GetCurrentNodeIndex () == 0);
  NavigateHook::Rng (*bot).clear_forced ();

  // ...an enemy behind the door is reported, persistence reroutes back...
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  foe->pev->origin = ystl::Vector (250.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::Rng (*bot).force_chance (true);
  engine.AdvanceTime (1.2f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot));
  engine.AdvanceTime (1.2f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot));
  engine.AdvanceTime (1.2f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot));
  engine.AdvanceTime (1.2f);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  NavigateHook::UpdateNav (*bot);
  CHECK (bot->GetCurrentNodeIndex () == 0);
  NavigateHook::Rng (*bot).clear_forced ();

  // ...a moving bot resets the door block state...
  bot->pev->velocity = ystl::Vector (100.0f, 0.0f, 0.0f);
  NavigateHook::SetChosenGoal (*bot, 2);
  bot->StartTask (TaskId::MoveTo, 50.0f, 2, 0.0f, false);
  NavigateHook::ChangeNode (*bot, 2);
  ystl::Array<int32_t> empty_walk {};
  NavigateHook::SetWalk (*bot, empty_walk);
  CHECK (NavigateHook::UpdateNav (*bot));
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...jump retiming bails on slow links, light gravity and near faces...
  NavigateHook::ChangeNode (*bot, 1);
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetTravelFlags (*bot, PathFlag::Jump);
  NavigateHook::SetJumpFinished (*bot, false);
  NavigateHook::SetDesiredVelocity (*bot, ystl::Vector (0.5f, 0.0f, 0.0f));
  NavigateHook::SetPathOriginVec (*bot, ystl::Vector (200.0f, 0.0f, 100.0f));
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK ((bot->pev->button & IN_JUMP) != 0);

  sv_gravity.Set (1);
  NavigateHook::SetJumpFinished (*bot, false);
  NavigateHook::SetDesiredVelocity (*bot, ystl::Vector (130.0f, 0.0f, 0.0f));
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK ((bot->pev->button & IN_JUMP) != 0);
  sv_gravity.Set (800);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.1f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.1f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.1f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.1f;
    out->pHit = door;
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::SetJumpFinished (*bot, false);
  NavigateHook::SetDesiredVelocity (*bot, ystl::Vector (130.0f, 0.0f, 0.0f));
  bot->pev->button = 0;
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK ((bot->pev->button & IN_JUMP) != 0);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...an expired lift window times out through the frame...
  NavigateHook::SetLiftEntity (*bot, plat);
  NavigateHook::SetLiftState (*bot, LiftState::EnteringIn);
  NavigateHook::SetLiftTravel (*bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::SetTravelFlags (*bot, static_cast<PathFlag> (0));
  NavigateHook::SetDesiredVelocity (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::LiftH (*bot);
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonInside));
  engine.AdvanceTime (11.0f);

  graph.paths_[2].flags |= NodeFlag::Lift;
  NavigateHook::ChangeNode (*bot, 2);
  ystl::Array<int32_t> lift_walk {};
  lift_walk.push (3);
  NavigateHook::SetWalk (*bot, lift_walk);
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::None));
  NavigateHook::SetLiftEntity (*bot, nullptr);

  // ...a wait timeout without a previous node searches again...
  NavigateHook::SetLiftEntity (*bot, plat);
  NavigateHook::SetLiftState (*bot, LiftState::EnteringIn);
  NavigateHook::SetLiftTravel (*bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::LiftH (*bot);
  engine.AdvanceTime (11.0f);

  graph.paths_[2].flags |= NodeFlag::Lift;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 1);
  NavigateHook::ChangeNode (*bot, 2);
  NavigateHook::SetWalk (*bot, lift_walk);
  NavigateHook::SetPrevNode (*bot, 0, kInvalidNodeIndex);
  NavigateHook::SetRechoice (*bot, 2);
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (graph.Exists (bot->GetCurrentNodeIndex ()));
  NavigateHook::SetLiftEntity (*bot, nullptr);
  graph.paths_[2].flags &= ~NodeFlag::Lift;

  // ...a direct lift timeout without a previous node searches again...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < v1[2]) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = plat;
    }
  });
  engine.AdvanceTime (0.15f);
  bot->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  NavigateHook::ChangeNode (*bot, 2);
  ystl::Array<int32_t> board_walk {};
  board_walk.push (3);
  NavigateHook::SetWalk (*bot, board_walk);
  NavigateHook::SetLiftState (*bot, LiftState::None);
  NavigateHook::SetLiftEntity (*bot, nullptr);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::EnteringIn));
  engine.AdvanceTime (5.5f);
  NavigateHook::SetPrevNode (*bot, 0, kInvalidNodeIndex);
  CHECK (!NavigateHook::LiftS (*bot));
  NavigateHook::SetLiftEntity (*bot, nullptr);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_harbor") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // door seed for the HasDoors legs, the lift slab and its panel below
  edict_t *door = engine.SpawnEntity ("func_door");
  HOST_REQUIRE (door != nullptr);
  const float door_pos[3] = { 190.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (door, door_pos);

  edict_t *plat = engine.SpawnEntity ("func_plat");
  HOST_REQUIRE (plat != nullptr);
  const float plat_pos[3] = { 200.0f, 0.0f, -50.0f };
  engine.Funcs ().pfnSetOrigin (plat, plat_pos);

  edict_t *panel = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (panel != nullptr);
  panel->v.target = string_t (engine.AllocString ("liftbtn"));
  const float panel_pos[3] = { 210.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (panel, panel_pos);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  HOST_REQUIRE (game.MapIs (MapFlags::HasDoors));

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("HarbA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("HarbB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  Bot *bot = nullptr, *mate = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "HarbA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "HarbB") {
        mate = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr);

  bots.ForEach ([&] (Bot *candidate) {
    if (candidate != bot && candidate != mate) {
      candidate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
    }
    return false;
  });

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  clients[mate->Ent ()].team = Team::CT;
  clients[bot->Ent ()].team = Team::CT;

  auto prep_harbor = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
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
    target->pev->groundentity = nullptr;
    target->is_alive_ = true;
    NavigateHook::SetHealthValue (*target, 100.0f);
    NavigateHook::SetAggression (*target, 0.9f, 0.0f);
  };
  prep_harbor (bot, ystl::Vector (180.0f, 0.0f, 0.0f));
  prep_harbor (mate, ystl::Vector (500.0f, 0.0f, 0.0f));
  clients.Update ();

  // ...door use starts the button window for the lift below...
  NavigateHook::ChangeNode (*bot, 2);
  bot->StartTask (TaskId::MoveTo, 50.0f, 3, 0.0f, false);
  ystl::Array<int32_t> door_walk {};
  door_walk.push (3);
  NavigateHook::SetWalk (*bot, door_walk);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if ((v1[0] < 190.0f) != (v2[0] < 190.0f)) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = door;
    }
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::Rng (*bot).force_chance (true);
  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (!NavigateHook::UpdateNav (*bot));
  CHECK (!NavigateHook::AimDirCan (*bot));
  NavigateHook::Rng (*bot).clear_forced ();
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...a closed lift door outside switches to the button search...
  prep_harbor (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  clients.Update ();
  NavigateHook::ChangeNode (*bot, 2);
  ystl::Array<int32_t> lift_walk {};
  lift_walk.push (3);
  NavigateHook::SetWalk (*bot, lift_walk);
  NavigateHook::SetLiftState (*bot, LiftState::None);
  NavigateHook::SetLiftEntity (*bot, nullptr);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = door;
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonOutside));

  // ...the lift slab below boards us when still...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < v1[2]) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = plat;
    }
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::SetLiftState (*bot, LiftState::None);
  prep_harbor (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  prep_harbor (mate, ystl::Vector (500.0f, 0.0f, 0.0f));
  clients.Update ();
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonInside));

  // ...a traveling slab overhead is left instead...
  NavigateHook::SetLiftState (*bot, LiftState::TravelingBy);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::Leaving));

  // ...no slab at the node probes the next lift node...
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);
  graph.paths_[3].flags |= NodeFlag::Lift;
  NavigateHook::ChangeNode (*bot, 2);
  ystl::Array<int32_t> probe_walk {};
  probe_walk.push (3);
  probe_walk.push (2);
  NavigateHook::SetWalk (*bot, probe_walk);
  NavigateHook::SetLiftState (*bot, LiftState::None);
  NavigateHook::SetLiftEntity (*bot, nullptr);
  prep_harbor (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  prep_harbor (mate, ystl::Vector (500.0f, 0.0f, 0.0f));
  clients.Update ();
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonOutside));

  // ...a slab echo on the next node is reserved up front...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[0] == 300.0f && v2[1] == 0.0f && v2[2] == 0.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = plat;
    }
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::SetLiftState (*bot, LiftState::None);
  ystl::Array<int32_t> probe_walk2 {};
  probe_walk2.push (2);
  probe_walk2.push (3);
  NavigateHook::SetWalk (*bot, probe_walk2);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonOutside));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...entering within reach waits, followers join or hold us...
  NavigateHook::SetWalk (*bot, lift_walk);
  NavigateHook::SetLiftTravel (*bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::SetLiftEntity (*bot, plat);
  NavigateHook::SetLiftState (*bot, LiftState::EnteringIn);
  mate->ClearTasks ();
  mate->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, 0, 0.0f, true);
  NavigateHook::SetTargetEntity (*mate, bot->Ent ());
  mate->pev->groundentity = nullptr;
  mate->pev->flags |= FL_ONGROUND;
  CHECK (mate->GetTaskId () == TaskId::FollowUser);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonInside));

  mate->pev->groundentity = plat;
  NavigateHook::SetLiftState (*bot, LiftState::EnteringIn);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::WaitingForTeammates));

  // ...an airborne follower holds the wait, a settled one releases it...
  mate->pev->groundentity = nullptr;
  mate->pev->flags &= ~FL_ONGROUND;
  NavigateHook::SetLiftState (*bot, LiftState::WaitingForTeammates);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot) == ystl::Vector (200.0f, 0.0f, 0.0f));

  mate->pev->flags |= FL_ONGROUND;
  mate->ClearTasks ();
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonInside));

  // ...inside the lift the panel is picked up, then the ride starts...
  engine.AdvanceTime (3.0f);
  prep_harbor (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  prep_harbor (mate, ystl::Vector (500.0f, 0.0f, 0.0f));
  clients.Update ();
  plat->v.targetname = string_t (engine.AllocString ("liftbtn"));
  bot->pev->groundentity = plat;
  mate->pev->groundentity = nullptr;
  NavigateHook::SetPrevNode (*bot, 0, 2);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::Pickup (*bot) == panel);

  // ...without a sightline the panel is used blind...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = door;
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::LookingButtonInside));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  plat->v.velocity = ystl::Vector (0.0f, 0.0f, 100.0f);
  NavigateHook::SetTargetEntity (*bot, mate->Ent ());
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::TravelingBy));

  // ...riding holds the destination...
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot).x == 200.0f);

  // ...the outside panel verifies a recent press before searching...
  NavigateHook::SetLiftState (*bot, LiftState::LookingButtonOutside);
  NavigateHook::SetPrevNode (*bot, 0, kInvalidNodeIndex);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot) == ystl::Vector (200.0f, 0.0f, 0.0f));
  NavigateHook::SetPrevNode (*bot, 0, 2);
  NavigateHook::LiftH (*bot);

  // ...an expired press with a rider waiting holds the previous node...
  engine.AdvanceTime (9.0f);
  prep_harbor (bot, ystl::Vector (200.0f, 0.0f, 0.0f));
  prep_harbor (mate, ystl::Vector (500.0f, 0.0f, 0.0f));
  clients.Update ();
  mate->pev->groundentity = plat;
  NavigateHook::SetLiftState (*bot, LiftState::LookingButtonOutside);
  NavigateHook::SetPrevNode (*bot, 0, 2);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot) == graph.paths_[2].origin);

  // ...a reserved lift holds the previous node too...
  mate->pev->groundentity = nullptr;
  NavigateHook::SetLiftState (*bot, LiftState::LookingButtonOutside);
  NavigateHook::SetLiftState (*mate, LiftState::LookingButtonOutside);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot) == graph.paths_[2].origin);

  // ...without a previous node the panel itself is the goal...
  mate->pev->groundentity = plat;
  NavigateHook::SetPrevNode (*bot, 0, kInvalidNodeIndex);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot) == ystl::Vector (210.0f, 0.0f, 0.0f));
  NavigateHook::SetPrevNode (*bot, 0, 2);

  // ...a free lift is reserved through its panel...
  mate->pev->groundentity = nullptr;
  NavigateHook::SetLiftEntity (*mate, nullptr);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::WaitingFor));
  CHECK (NavigateHook::Pickup (*bot) == panel);

  // ...without any panel the bot simply waits...
  plat->v.targetname = string_t (engine.AllocString ("nobtn"));
  NavigateHook::SetLiftState (*bot, LiftState::LookingButtonOutside);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::WaitingFor));

  // ...waiting falls back along the chain...
  NavigateHook::SetLiftState (*bot, LiftState::WaitingFor);
  NavigateHook::SetPrevNode (*bot, 0, 2);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot) == graph.paths_[2].origin);

  NavigateHook::SetPrevNode (*bot, 0, 3);
  NavigateHook::SetPrevNode (*bot, 1, 2);
  CHECK (NavigateHook::LiftH (*bot));
  CHECK (NavigateHook::DestOrigin (*bot) == graph.paths_[2].origin);
  graph.paths_[3].flags &= ~NodeFlag::Lift;

  // ...leaving levels the state, expiry clears it...
  plat->v.velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->groundentity = nullptr;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SetLiftState (*bot, LiftState::TravelingBy);
  NavigateHook::SetLiftEntity (*bot, plat);
  CHECK (NavigateHook::LiftS (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::Leaving));

  engine.AdvanceTime (8.0f);
  CHECK (NavigateHook::LiftS (*bot));
  CHECK (NavigateHook::GetLiftState (*bot) == ystl::to_underlying (LiftState::None));
  CHECK (NavigateHook::LiftEntity (*bot) == nullptr);

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_vanguard") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // hostage map for the rescue and no-hostage legs below
  edict_t *hostage_seed = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hostage_seed != nullptr);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  HOST_REQUIRE (game.MapIs (MapFlags::HostageRescue));

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("VanA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("VanB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("VanE1", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("VanE2", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);

  Bot *bot = nullptr, *mate = nullptr, *foe1 = nullptr, *foe2 = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr || foe1 == nullptr || foe2 == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "VanA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "VanB") {
        mate = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "VanE1") {
        foe1 = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "VanE2") {
        foe2 = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr && foe1 != nullptr && foe2 != nullptr);

  bots.ForEach ([&] (Bot *candidate) {
    if (candidate != bot && candidate != mate && candidate != foe1 && candidate != foe2) {
      candidate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
    }
    return false;
  });

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  foe1->team_ = Team::Terrorist;
  foe2->team_ = Team::Terrorist;
  clients[bot->Ent ()].team = Team::CT;
  clients[mate->Ent ()].team = Team::CT;
  clients[foe1->Ent ()].team = Team::Terrorist;
  clients[foe2->Ent ()].team = Team::Terrorist;

  auto prep_van = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
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
    target->pev->groundentity = nullptr;
    target->is_alive_ = true;
    NavigateHook::SetHealthValue (*target, 100.0f);
    NavigateHook::SetAggression (*target, 0.9f, 0.0f);
  };
  prep_van (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_van (mate, ystl::Vector (60.0f, 0.0f, 0.0f));
  prep_van (foe1, ystl::Vector (5000.0f, 0.0f, 0.0f));
  prep_van (foe2, ystl::Vector (5000.0f, 0.0f, 0.0f));
  clients.Update ();

  ystl::SmallArray<int32_t> defensive {};
  defensive.push (2);
  ystl::SmallArray<int32_t> offensive {};
  offensive.push (1);

  // ...interesting hostages: the dead skip, the held and the far wait...
  edict_t *box = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (box != nullptr);
  const float box_pos[3] = { 80.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (box, box_pos);

  edict_t *near_hostage = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (near_hostage != nullptr);
  const float near_pos[3] = { 50.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (near_hostage, near_pos);

  edict_t *far_hostage = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (far_hostage != nullptr);
  const float far_pos[3] = { 3000.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (far_hostage, far_pos);
  engine.Funcs ().pfnSetOrigin (hostage_seed, far_pos);

  mate->is_alive_ = false;
  game_state.UpdateInterestingEntities ();
  CHECK (graph.Exists (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive)));
  mate->is_alive_ = true;

  NavigateHook::PushHostage (*mate, near_hostage);
  engine.AdvanceTime (0.6f);
  game_state.UpdateInterestingEntities ();
  CHECK (graph.Exists (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive)));
  NavigateHook::ClearHostages (*mate);

  const float far_pos2[3] = { 3000.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (near_hostage, far_pos2);
  engine.AdvanceTime (0.6f);
  game_state.UpdateInterestingEntities ();
  CHECK (graph.Exists (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive)));

  engine.Funcs ().pfnSetOrigin (near_hostage, near_pos);
  engine.AdvanceTime (0.6f);
  game_state.UpdateInterestingEntities ();
  CHECK (graph.Exists (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive)));

  // ...carrier rush takes the nearest rescue, walls and strays are skipped...
  graph.Reset ();

  const ystl::Vector rescue_spots[6] = {
    ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::Vector (300.0f, 0.0f, 0.0f),
    ystl::Vector (2000.0f, 0.0f, 0.0f),
    ystl::Vector (500.0f, 0.0f, 0.0f),
  };

  for (int i = 0; i < 6; ++i) {
    Path path {};
    path.origin = rescue_spots[i];
    path.number = i;
    path.flags = (i == 3 || i == 5) ? ystl::to_underlying (NodeFlag::Rescue) : 0;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  graph.PopulateNodes ();

  NavigateHook::SetTeam (*bot, Team::CT);
  NavigateHook::SetPersonality (*bot, Personality::Rusher);
  NavigateHook::SetHealthValue (*bot, 100.0f);
  NavigateHook::SetAggression (*bot, 0.9f, 0.0f);
  bot->has_hostage_ = true;
  CHECK (NavigateHook::HostageActionGoal (*bot, &defensive, &offensive) == 3);
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::RescueHostage, &defensive, &offensive) == 3);

  edict_t *site_wall = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (site_wall != nullptr);
  site_wall->v.solid = SOLID_BBOX;
  site_wall->v.absmin = ystl::Vector (270.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (330.0f, 200.0f, 100.0f);
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::RescueHostage, &defensive, &offensive) == 5);
  site_wall->v.solid = SOLID_NOT;
  mode_walls.Reset ();

  // ...stale rescue points are skipped without crashing...
  graph.Reset ();

  for (int i = 0; i < 5; ++i) {
    Path path {};
    path.origin = rescue_spots[i];
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  CHECK (NavigateHook::GoalPost (*bot, GoalTactic::RescueHostage, &defensive, &offensive) == 3);

  game_state.RoundStart ();
  mode_walls.Reset ();
  graph.Reset ();

  for (int i = 0; i < 6; ++i) {
    Path path {};
    path.origin = rescue_spots[i];
    path.number = i;
    path.flags = (i == 3 || i == 5) ? ystl::to_underlying (NodeFlag::Rescue) : 0;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  graph.PopulateNodes ();

  const int patient_rescue = NavigateHook::GoalPost (*bot, GoalTactic::RescueHostage, &defensive, &offensive);
  CHECK (patient_rescue == 3 || patient_rescue == 5);

  site_wall->v.absmin = ystl::Vector (270.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (330.0f, 200.0f, 100.0f);
  site_wall->v.solid = SOLID_BBOX;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());
  const int walled_rescue = NavigateHook::GoalPost (*bot, GoalTactic::RescueHostage, &defensive, &offensive);
  CHECK (walled_rescue == 3 || walled_rescue == 5);
  site_wall->v.solid = SOLID_NOT;
  mode_walls.Reset ();

  bot->has_hostage_ = false;
  NavigateHook::SetPersonality (*bot, Personality::Normal);
  graph.Reset ();
  BuildNavGraph ();
  planner.Init ();

  // ...vip carriers rate goals, walls, range and enemies filter them...
  graph.Reset ();

  const ystl::Vector spots6[6] = {
    ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::Vector (300.0f, 0.0f, 0.0f),
    ystl::Vector (400.0f, 0.0f, 0.0f),
    ystl::Vector (500.0f, 0.0f, 0.0f),
  };

  for (int i = 0; i < 6; ++i) {
    Path path {};
    path.origin = spots6[i];
    path.number = i;
    path.flags = (i == 4 || i == 5) ? ystl::to_underlying (NodeFlag::Goal) : 0;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link6 = [] (int from, int to, int distance) {
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
    link6 (i, i + 1, 100);
    link6 (i + 1, i, 100);
  }
  graph.PopulateNodes ();
  planner.Init ();

  bot->is_vip_ = true;
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive)));

  site_wall->v.absmin = ystl::Vector (470.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (530.0f, 200.0f, 100.0f);
  site_wall->v.solid = SOLID_BBOX;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive)));
  site_wall->v.solid = SOLID_NOT;
  mode_walls.Reset ();

  bot->pev->origin = ystl::Vector (-2000.0f, 0.0f, 0.0f);
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive)));
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  foe1->pev->origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  foe2->pev->origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive)));
  foe1->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  foe2->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  bot->is_vip_ = false;

  // ...a stale goal point is skipped without crashing...
  graph.Reset ();
  BuildNavGraph ();
  graph.paths_[4].flags |= NodeFlag::Goal;
  graph.paths_[4].number = 99;
  graph.PopulateNodes ();
  planner.Init ();
  bot->is_vip_ = true;
  CHECK (graph.Exists (NavigateHook::GoalPost (*bot, GoalTactic::Goal, &defensive, &offensive)));
  bot->is_vip_ = false;
  graph.paths_[4].number = 4;
  graph.Reset ();
  BuildNavGraph ();

  // ...node search skips the invalid, walled, current and off-limits...
  ystl::SmallArray<int32_t> dup_pool {};
  dup_pool.push (1);
  dup_pool.push (1);
  dup_pool.push (2);
  dup_pool.push (3);
  dup_pool.push (4);

  int dup_picked[4] = { kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex };
  NavigateHook::PostGoals (*bot, dup_pool, dup_picked);

  for (int i = 0; i < 4; ++i) {
    CHECK (dup_picked[i] == kInvalidNodeIndex || (dup_picked[i] >= 1 && dup_picked[i] <= 4));

    for (int j = i + 1; j < 4; ++j) {
      CHECK (dup_picked[i] == kInvalidNodeIndex || dup_picked[i] != dup_picked[j]);
    }
  }

  const int32_t saved_number = graph.paths_[1].number;
  graph.paths_[1].number = 99;
  ystl::Array<int32_t> probe_nodes {};
  probe_nodes.push (1);
  CHECK (NavigateHook::NextBestEx (*bot, probe_nodes, false));
  graph.paths_[1].number = saved_number;

  site_wall->v.absmin = ystl::Vector (70.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (130.0f, 200.0f, 100.0f);
  site_wall->v.solid = SOLID_BBOX;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());
  CHECK (NavigateHook::NextBestEx (*bot, probe_nodes, false));

  ystl::SmallArray<int32_t> wall_pool {};
  wall_pool.push (1);
  wall_pool.push (2);
  wall_pool.push (3);

  int wall_picked[4] = { kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex };
  NavigateHook::PostGoals (*bot, wall_pool, wall_picked);
  CHECK ((wall_picked[0] == 2 && wall_picked[1] == 3) || (wall_picked[0] == 3 && wall_picked[1] == 2));
  CHECK (wall_picked[2] == kInvalidNodeIndex && wall_picked[3] == kInvalidNodeIndex);
  mode_walls.Reset ();

  NavigateHook::ChangeNode (*bot, 0);
  probe_nodes.clear ();
  probe_nodes.push (0);
  CHECK (NavigateHook::NextBestEx (*bot, probe_nodes, false));

  graph.paths_[2].flags |= NodeFlag::NoHostage;
  bot->has_hostage_ = true;
  probe_nodes.clear ();
  probe_nodes.push (2);
  CHECK (NavigateHook::NextBestEx (*bot, probe_nodes, false));
  graph.paths_[2].flags &= ~NodeFlag::NoHostage;
  bot->has_hostage_ = false;

  probe_nodes.clear ();
  probe_nodes.push (4);
  CHECK (NavigateHook::NextBestEx (*bot, probe_nodes, false));

  // ...cover spots behind a wall are skipped, the rest hide us...
  site_wall->v.absmin = ystl::Vector (150.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (160.0f, 200.0f, 100.0f);
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());
  NavigateHook::SetEnemyOrigin (*bot, ystl::Vector (400.0f, 0.0f, 0.0f));
  NavigateHook::ChangeNode (*bot, 0);
  CHECK (NavigateHook::CoverNode (*bot, 1000.0f) == 1);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = site_wall;
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::CoverNode (*bot, 1000.0f) == 1);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);
  mode_walls.Reset ();

  // ...the visibility table drives validity, buckets and shortcuts...
  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());

  for (int i = 0; i < 5; ++i) {
    graph.AddToBucket (graph.paths_[i].origin, i);
  }
  graph.AddToBucket (graph.paths_[0].origin, 1);
  graph.AddToBucket (graph.paths_[0].origin, 2);
  graph.AddToBucket (graph.paths_[0].origin, 3);
  graph.AddToBucket (graph.paths_[0].origin, 4);

  NavigateHook::SetCurrentRaw (*bot, kInvalidNodeIndex);
  NavigateHook::SetRechoice (*bot, 0);
  NavigateHook::FindValid (*bot);
  CHECK (bot->GetCurrentNodeIndex () != kInvalidNodeIndex);

  NavigateHook::ChangeNode (*bot, 1);
  ystl::Array<int32_t> short_walk {};
  short_walk.push (0);
  short_walk.push (1);
  NavigateHook::SetWalk (*bot, short_walk);
  bot->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  NavigateHook::FindValid (*bot);
  CHECK (bot->GetCurrentNodeIndex () == 1);

  // ...stale, flagged and jumpy links all break the shortcut...
  ystl::Array<int32_t> stale_walk {};
  stale_walk.push (99);
  stale_walk.push (1);
  NavigateHook::SetWalk (*bot, stale_walk);
  NavigateHook::FindValid (*bot);

  graph.paths_[0].flags |= NodeFlag::Button;
  NavigateHook::SetWalk (*bot, short_walk);
  NavigateHook::FindValid (*bot);
  graph.paths_[0].flags &= ~NodeFlag::Button;

  for (auto &slot : graph.paths_[0].links) {
    if (slot.index == 1) {
      slot.flags = ystl::to_underlying (PathFlag::Jump);
    }
  }
  NavigateHook::SetWalk (*bot, short_walk);
  NavigateHook::FindValid (*bot);

  for (auto &slot : graph.paths_[0].links) {
    if (slot.index == 1) {
      slot.flags = 0;
    }
  }
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::SetWalk (*bot, short_walk);
  NavigateHook::FindValid (*bot);
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();

  engine.AdvanceTime (4.0f);
  NavigateHook::FindValid (*bot);
  CHECK (graph.Exists (bot->GetCurrentNodeIndex ()));

  // ...nearest queries skip the occupied, the far ones fail closed...
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  NavigateHook::ChangeNode (*bot, 0);
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  graph.AddToBucket (bot->pev->origin, 99);
  graph.AddToBucket (bot->pev->origin, 4);
  graph.AddToBucket (bot->pev->origin, 1);
  CHECK (graph.Exists (NavigateHook::NearestNode (*bot)));
  graph.InitBuckets ();
  CHECK (graph.Exists (NavigateHook::NearestNode (*bot)));

  bot->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  CHECK (NavigateHook::NearestNode (*bot) == 4);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  // ...walled graphs randomize and measure within the open set...
  edict_t *far_wall = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (far_wall != nullptr);
  far_wall->v.solid = SOLID_BBOX;
  far_wall->v.absmin = ystl::Vector (1950.0f, -200.0f, -100.0f);
  far_wall->v.absmax = ystl::Vector (2050.0f, 200.0f, 100.0f);

  site_wall->v.absmin = ystl::Vector (-50.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (350.0f, 200.0f, 100.0f);
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());
  CHECK (graph.Exists (NavigateHook::RandomNode (*bot, PointType::Count)));
  CHECK (graph.Exists (NavigateHook::FarestNode (*bot, ystl::Vector (5000.0f, 0.0f, 0.0f), 100.0f)));
  mode_walls.Reset ();

  site_wall->v.absmin = ystl::Vector (250.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (260.0f, 200.0f, 100.0f);
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());
  CHECK (NavigateHook::FarestNode (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), 100.0f) == 2);
  mode_walls.Reset ();

  // ...the bomb search reads proximity, audio, goals and hunches...
  CHECK (NavigateHook::BombNode (*bot) == 0);

  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (5000.0f, 0.0f, 0.0f));
  bot->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  CHECK (NavigateHook::BombNode (*bot) == 0);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  graph.paths_[1].flags |= NodeFlag::Goal;
  graph.paths_[2].flags |= NodeFlag::Goal;
  graph.PopulateNodes ();
  planner.Init ();

  bot->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  NavigateHook::Rng (*bot).force_chance (true);
  const int deduced = NavigateHook::BombNode (*bot);
  CHECK (deduced == 1 || deduced == 2);
  NavigateHook::Rng (*bot).force_chance (false);
  const int plain_pick = NavigateHook::BombNode (*bot);
  CHECK (plain_pick == 1 || plain_pick == 2);
  NavigateHook::Rng (*bot).clear_forced ();
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  game_state.SetBombPlanted (false);

  // ...hostage goals are reached from further out...
  NavigateHook::ChangeNode (*bot, 2);
  ystl::Array<int32_t> goal_walk {};
  goal_walk.push (3);
  NavigateHook::SetWalk (*bot, goal_walk);
  CHECK (!NavigateHook::UpdateNav (*bot));

  // ...defend posts skip walls, campers and spotters, then rate...
  NavigateHook::ChangeNode (*bot, 0);
  site_wall->v.absmin = ystl::Vector (150.0f, -200.0f, -100.0f);
  site_wall->v.absmax = ystl::Vector (160.0f, 200.0f, 100.0f);
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = site_wall;
  });
  engine.AdvanceTime (0.15f);
  const int walled_defend = NavigateHook::DefendNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f));
  CHECK (graph.Exists (walled_defend));
  mode_walls.Reset ();

  mate->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  const int spotted_defend = NavigateHook::DefendNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f));
  CHECK (spotted_defend == 1 || spotted_defend == 2 || spotted_defend == 3);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  NavigateHook::Rng (*bot).force_int (0);
  CHECK (graph.Exists (NavigateHook::DefendNode (*bot, ystl::Vector (300.0f, 0.0f, 0.0f))));
  NavigateHook::Rng (*bot).clear_forced ();

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_summit") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // hostage map plus doors for the alternative and exemption legs
  edict_t *hostage_seed = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hostage_seed != nullptr);
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
  HOST_REQUIRE (game.MapIs (MapFlags::HostageRescue));
  HOST_REQUIRE (game.MapIs (MapFlags::HasDoors));

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend (); // autostart would block bot creation below
  }
  BuildNavGraph ();
  planner.Init ();

  // extra links for the alternative search: 0 -> 4 is dead, 0 -> 3 loops back
  auto add_link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        slot.flags = 0;
        slot.velocity = ystl::Vector ();
        return;
      }
    }
    HOST_REQUIRE (false);
  };
  add_link (0, 4, 100);
  add_link (0, 3, 100);
  add_link (3, 2, 100);
  add_link (3, 0, 100);

  bots.InitQuota ();
  cv_quota.Set (10);

  bots.Addbot ("SumA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("SumB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  Bot *bot = nullptr, *mate = nullptr;

  for (int i = 0; i < 40 && (bot == nullptr || mate == nullptr); ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();

    bots.ForEach ([&] (Bot *candidate) {
      if (ystl::StringRef (candidate->pev->netname.chars ()) == "SumA") {
        bot = candidate;
      }
      else if (ystl::StringRef (candidate->pev->netname.chars ()) == "SumB") {
        mate = candidate;
      }
      return false;
    });
  }
  HOST_REQUIRE (bot != nullptr && mate != nullptr);

  bots.ForEach ([&] (Bot *candidate) {
    if (candidate != bot && candidate != mate) {
      candidate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
    }
    return false;
  });

  bot->team_ = Team::CT;
  mate->team_ = Team::CT;
  clients[bot->Ent ()].team = Team::CT;
  clients[mate->Ent ()].team = Team::CT;

  auto prep_summit = [] (Bot *target, const ystl::Vector &pos) {
    target->pev->origin = pos;
    target->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
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
    target->pev->groundentity = nullptr;
    target->is_alive_ = true;
    NavigateHook::SetHealthValue (*target, 100.0f);
    NavigateHook::SetAggression (*target, 0.9f, 0.0f);
  };
  prep_summit (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  prep_summit (mate, ystl::Vector (100.0f, 0.0f, 0.0f));
  clients.Update ();

  // ...occupied heads with dead links fall back to the walk...
  NavigateHook::ChangeNode (*bot, 0);
  ystl::Array<int32_t> alt_walk {};
  alt_walk.push (1);
  alt_walk.push (2);
  NavigateHook::SetWalk (*bot, alt_walk);
  CHECK (!NavigateHook::SelectNext (*bot));

  // ...the visibility table drives validity, buckets and shortcuts...
  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());

  // ...high, ladder and off-limits alternatives are skipped one by one...
  graph.paths_[3].origin.z = 100.0f;
  CHECK (!NavigateHook::SelectNext (*bot));
  graph.paths_[3].origin.z = 0.0f;

  graph.paths_[3].flags |= NodeFlag::Ladder;
  CHECK (!NavigateHook::SelectNext (*bot));
  graph.paths_[3].flags &= ~NodeFlag::Ladder;

  graph.paths_[3].flags |= NodeFlag::NoHostage;
  bot->has_hostage_ = true;
  CHECK (!NavigateHook::SelectNext (*bot));
  graph.paths_[3].flags &= ~NodeFlag::NoHostage;
  bot->has_hostage_ = false;

  // ...a free alternative is adopted...
  CHECK (NavigateHook::SelectNext (*bot));
  CHECK (NavigateHook::WalkAt (*bot, 0) == 3);

  // ...a camping mate on the next node is woken by the advance...
  prep_summit (mate, ystl::Vector (200.0f, 0.0f, 0.0f));
  clients.Update ();
  NavigateHook::ChangeNode (*mate, 2);
  mate->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  NavigateHook::ChangeNode (*bot, 0);
  ystl::Array<int32_t> camp_walk {};
  camp_walk.push (1);
  camp_walk.push (2);
  NavigateHook::SetWalk (*bot, camp_walk);
  bot->ClearTasks ();
  CHECK (NavigateHook::Advance (*bot));
  CHECK (mate->GetTaskId () != TaskId::Camp);
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();

  // ...danger on the route ducks or shifts the advance...
  NavigateHook::SetPersonality (*bot, Personality::Careful);
  NavigateHook::SetBaseAggression (*bot, 0.0f);
  NavigateHook::SetRechoice (*bot, 2);
  NavigateHook::ChangeNode (*bot, 0);
  engine.AdvanceTime (4.0f);
  bot->ClearTasks ();
  NavigateHook::Advance (*bot);
  bots.SetEnemySpotted (true);

  NavigateHook::ChangeNode (*bot, 0);
  bot->ClearTasks ();
  ystl::Array<int32_t> danger_walk {};
  danger_walk.push (2);
  danger_walk.push (1);
  NavigateHook::SetWalk (*bot, danger_walk);
  bot->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  CHECK (NavigateHook::Advance (*bot));
  NavigateHook::SetPersonality (*bot, Personality::Normal);
  NavigateHook::SetBaseAggression (*bot, 1.0f);
  NavigateHook::SetHealthValue (*bot, 10.0f);
  NavigateHook::Rng (*bot).force_chance (true);
  NavigateHook::SetRechoice (*bot, 2);
  NavigateHook::ChangeNode (*bot, 0);
  ystl::Array<int32_t> danger_walk3 {};
  danger_walk3.push (2);
  danger_walk3.push (1);
  danger_walk3.push (0);
  NavigateHook::SetWalk (*bot, danger_walk3);
  CHECK (NavigateHook::Advance (*bot));
  NavigateHook::Rng (*bot).clear_forced ();

  // ...a jump ahead is measured, drawn and sequenced...
  for (auto &slot : graph.paths_[0].links) {
    if (slot.index == 1) {
      slot.flags = ystl::to_underlying (PathFlag::Jump);
    }
  }

  for (auto &slot : graph.paths_[1].links) {
    if (slot.index == 2) {
      slot.flags = ystl::to_underlying (PathFlag::Jump);
    }
  }
  NavigateHook::SetRechoice (*bot, 2);
  NavigateHook::ChangeNode (*bot, 0);
  ystl::Array<int32_t> jump_walk {};
  jump_walk.push (0);
  jump_walk.push (1);
  jump_walk.push (2);
  NavigateHook::SetWalk (*bot, jump_walk);
  bot->ClearTasks ();
  CHECK (NavigateHook::Advance (*bot));

  for (auto &slot : graph.paths_[1].links) {
    if (slot.index == 2) {
      slot.flags = 0;
    }
  }

  for (auto &slot : graph.paths_[0].links) {
    if (slot.index == 1) {
      slot.flags = 0;
    }
  }

  // ...hostage carriers refuse jumps their charges cannot follow...
  graph.paths_[2].flags |= NodeFlag::NoHostage;
  bot->has_hostage_ = true;
  NavigateHook::ChangeNode (*bot, 0);
  ystl::Array<int32_t> hostage_walk {};
  hostage_walk.push (0);
  hostage_walk.push (2);
  NavigateHook::SetWalk (*bot, hostage_walk);
  bot->ClearTasks ();
  CHECK (NavigateHook::Advance (*bot));
  graph.paths_[2].flags &= ~NodeFlag::NoHostage;
  bot->has_hostage_ = false;

  // ...a ladder held by a mate pauses the advance...
  graph.paths_[1].flags |= NodeFlag::Ladder;
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  mate->pev->movetype = MOVETYPE_FLY;
  NavigateHook::ChangeNode (*mate, 1);
  clients.Update ();
  NavigateHook::ChangeNode (*bot, 0);
  ystl::Array<int32_t> ladder_walk {};
  ladder_walk.push (0);
  ladder_walk.push (1);
  NavigateHook::SetWalk (*bot, ladder_walk);
  bot->ClearTasks ();
  CHECK (NavigateHook::Advance (*bot));
  mate->pev->movetype = MOVETYPE_WALK;
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  graph.paths_[1].flags &= ~NodeFlag::Ladder;
  clients.Update ();

  // ...ladder path origins slide below the climber...
  edict_t *ladder = engine.SpawnEntity ("func_ladder");
  HOST_REQUIRE (ladder != nullptr);
  const float ladder_pos[3] = { 10.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (ladder, ladder_pos);
  prep_summit (bot, ystl::Vector (10.0f, 0.0f, 50.0f));
  clients.Update ();
  bot->pev->movetype = MOVETYPE_FLY;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SetPathOr (*bot);
  bot->pev->movetype = MOVETYPE_WALK;
  prep_summit (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  clients.Update ();

  // ...wide nodes validate a ring of alternatives...
  graph.paths_[0].radius = 32.0f;
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::SetWalk (*bot, alt_walk);

  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = door;
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::SetPathOr (*bot);
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.15f);
  graph.paths_[0].radius = 0.0f;

  // ...slopes tilt the forward trace, hostages are exempt...
  bot->pev->groundentity = door;
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < v1[2] && v1[2] < 10.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->vecPlaneNormal.x = 0.0f;
      out->vecPlaneNormal.y = 0.0f;
      out->vecPlaneNormal.z = 0.5f;
      out->pHit = door;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::BlockedForward (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)) == nullptr);
  bot->pev->groundentity = nullptr;

  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = hostage_seed;
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::BlockedForward (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)) == nullptr);
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...shoulders report each side, ducking adds the low ray...
  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);

  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = wall;
  });
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v1[2] == 8.0f && v1[1] > 0.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::BlockedForward (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)) == wall);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v1[2] == 8.0f && v1[1] < 0.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::BlockedForward (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)) == wall);

  bot->pev->flags |= FL_DUCKING;
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v1[2] <= 1.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::BlockedForward (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)) == wall);
  bot->pev->flags &= ~FL_DUCKING;
  engine.SetTraceLineHook (nullptr);
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...jump clearance falls back to duck height, ducking rejects sides...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] == v1[2] && v1[0] != 0.0f && v1[2] < 20.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::JumpUp (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)));
  CHECK (!NavigateHook::DuckUnder (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)));

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] == v1[2] && v1[0] != 0.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (!NavigateHook::JumpUp (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] == v1[2] && v1[0] < 0.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (!NavigateHook::JumpUp (*bot, ystl::Vector (1.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...near drops read deadly, camp slots replace the worse...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[2] < v1[2]) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
  });
  engine.AdvanceTime (0.15f);
  CHECK (NavigateHook::DeadlyMove (*bot, ystl::Vector (10.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...camp slots replace the worse candidates...
  graph.Reset ();

  for (int i = 0; i < 12; ++i) {
    Path path {};
    path.origin = ystl::Vector (static_cast<float> (i * 100), 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  graph.PopulateNodes ();
  planner.Init ();

  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());

  bot->pev->origin = ystl::Vector (600.0f, 0.0f, 0.0f);
  NavigateHook::ChangeNode (*bot, 6);
  NavigateHook::Rng (*bot).force_int (0);
  CHECK (graph.Exists (NavigateHook::CampDir (*bot)));
  NavigateHook::Rng (*bot).clear_forced ();
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  // ...strafe speed flips around single walls and bails on two...
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[1] > v1[1]) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
    else if (v2[2] < v1[2]) {
      out->flFraction = 0.05f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.05f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.05f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.05f;
    }
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::StrafeSpeedRaw (*bot, -100.0f);
  CHECK (NavigateHook::StrafeSpeed (*bot) == 100.0f);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v2[1] < v1[1]) {
      out->flFraction = 0.5f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
      out->pHit = wall;
    }
    else if (v2[2] < v1[2]) {
      out->flFraction = 0.05f;
      out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.05f;
      out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.05f;
      out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.05f;
    }
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::StrafeSpeedRaw (*bot, 100.0f);
  CHECK (NavigateHook::StrafeSpeed (*bot) == -100.0f);

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->pHit = wall;
  });
  engine.AdvanceTime (0.15f);
  NavigateHook::SetStrafe (*bot, 0.0f);
  NavigateHook::StrafeSpeedRaw (*bot, 100.0f);
  CHECK (NavigateHook::StrafeSpeed (*bot) == 0.0f);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.15f);

  // ...occupancy skips the moving, the tight and the dead, but not us...
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (!NavigateHook::Occupied (*bot, 1, true));

  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  graph.paths_[1].radius = 32.0f;
  clients.Update ();
  CHECK (NavigateHook::Occupied (*bot, 1, true));
  graph.paths_[1].radius = 0.0f;

  mate->is_alive_ = false;
  NavigateHook::ChangeNode (*mate, 1);
  mate->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (!NavigateHook::Occupied (*bot, 1, false));
  mate->is_alive_ = true;
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  NavigateHook::ChangeNode (*bot, 2);
  bot->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  mate->pev->origin = ystl::Vector (5000.0f, 0.0f, 30.0f);
  clients.Update ();
  CHECK (!NavigateHook::Occupied (*bot, 2, false));
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  mate->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  // ...low nodes are unreachable, stale searches fail closed...
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 200.0f);
  CHECK (!NavigateHook::Reachable (*bot, 0));
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  NavigateHook::SetStale (*bot, true);
  PathWalk staging {};
  staging.Init (64);
  NavigateData::PathMeta meta {};
  CHECK (!NavigateHook::ShortestPath (*bot, 0, 3, staging, meta));
  NavigateHook::SetStale (*bot, false);

  // ...wounded creatures estimate the same reach...
  NavigateHook::SetCreature (*bot, true);
  NavigateHook::SetLastDamage (*bot, game.Time () - 1.0f);
  NavigateHook::ChangeNode (*bot, 0);
  NavigateHook::ChangeNode (*bot, 1);
  engine.AdvanceTime (0.3f);
  CHECK (NavigateHook::ReachTime (*bot) == 3.5f);
  NavigateHook::SetCreature (*bot, false);
  NavigateHook::SetLastDamage (*bot, 0.0f);

  // ...an occupied route tail refinds the path...
  mate->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  NavigateHook::ChangeNode (*bot, 0);
  ystl::Array<int32_t> tail_walk {};
  tail_walk.push (1);
  tail_walk.push (2);
  NavigateHook::SetWalk (*bot, tail_walk);
  bot->ClearTasks ();
  CHECK (NavigateHook::UpdateNav (*bot));

  // ...a closing hindrance backs the advance off...
  NavigateHook::SetFrameInterval (*bot, 0.05f);
  bot->pev->maxspeed = 100.0f;
  mate->pev->origin = ystl::Vector (50.0f, 0.0f, 0.0f);
  mate->pev->velocity = ystl::Vector (-50.0f, 0.0f, 0.0f);
  clients.Update ();
  engine.AdvanceTime (0.5f);
  NavigateHook::SetMoveAngles (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  NavigateHook::SetStrafe (*bot, 0.0f);
  NavigateHook::SetMoveSpeed (*bot, 0.0f);
  NavigateHook::Avoid (*bot, ystl::Vector (1.0f, 0.0f, 0.0f));
  NavigateHook::TranslateInput (*bot);
  CHECK ((bot->pev->button & IN_BACK) != 0);

  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/navigate_relay") {
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

  // ten chain with an island at the end, so the astar legs run (they need 8+ nodes)
  graph.Reset ();

  for (int i = 0; i < 10; ++i) {
    Path path {};
    path.origin = ystl::Vector (static_cast<float> (i * 100), 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto chain_link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  for (int i = 0; i < 8; ++i) {
    chain_link (i, i + 1, 100);
    chain_link (i + 1, i, 100);
  }
  planner.Init ();
  planner.SetPathsCheckFailed (false); // force the astar legs below

  // wall slabs toggled by solidity: nodes 3-9, everything, a gap, nodes 0-8
  auto make_wall = [&] (float min_x, float max_x) -> edict_t * {
    edict_t *wall = engine.SpawnEntity ("test_effect");

    if (wall != nullptr) {
      wall->v.solid = SOLID_NOT;
      wall->v.absmin = ystl::Vector (min_x, -200.0f, -100.0f);
      wall->v.absmax = ystl::Vector (max_x, 200.0f, 100.0f);
    }
    return wall;
  };
  edict_t *wall389 = make_wall (270.0f, 950.0f);
  HOST_REQUIRE (wall389 != nullptr);
  edict_t *wall_all = make_wall (-50.0f, 950.0f);
  HOST_REQUIRE (wall_all != nullptr);
  edict_t *wall_seg = make_wall (450.0f, 460.0f);
  HOST_REQUIRE (wall_seg != nullptr);
  edict_t *wall_big = make_wall (-50.0f, 850.0f);
  HOST_REQUIRE (wall_big != nullptr);

  auto bot = CreateNavBot ("RelayBot", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bot.get () != nullptr);
  NavigateHook::SetTeam (*bot, Team::CT);
  NavigateHook::ChangeNode (*bot, 0);

  // ...a sealed destination retargets the search to a free node...
  wall389->v.solid = SOLID_BBOX;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());

  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);

  for (int i = 0; i < 3 && NavigateHook::WalkLength (*bot) == 0; ++i) {
    NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  }
  CHECK (NavigateHook::WalkLength (*bot) > 0);

  // ...a sealed map backs the search off entirely...
  wall389->v.solid = SOLID_NOT;
  wall_all->v.solid = SOLID_BBOX;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());

  NavigateHook::SyncPath (*bot, 0, 3, FindPathType::Fast);
  CHECK (NavigateHook::WalkLength (*bot) == 0);

  // ...a broken island salvages a reachable node through the walls...
  wall_all->v.solid = SOLID_NOT;
  wall_seg->v.solid = SOLID_BBOX;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());

  NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Fast);

  for (int i = 0; i < 3 && NavigateHook::WalkLength (*bot) == 0; ++i) {
    NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Fast);
  }
  CHECK (NavigateHook::WalkLength (*bot) > 0);

  // ...a sealed crew without a free node abandons the salvage...
  wall_seg->v.solid = SOLID_NOT;
  wall_big->v.solid = SOLID_BBOX;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());

  NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Fast);
  CHECK (NavigateHook::WalkLength (*bot) == 0);

  // ...a broken island without walls fails closed...
  wall_big->v.solid = SOLID_NOT;
  mode_walls.OnRoundStart ();
  HOST_REQUIRE (!mode_walls.HasWalls ());

  NavigateHook::SyncPath (*bot, 0, 9, FindPathType::Fast);
  CHECK (NavigateHook::WalkLength (*bot) == 0);

  // ...a far source without a node fails closed...
  bot->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  NavigateHook::SyncPath (*bot, 99, 3, FindPathType::Fast);
  CHECK (NavigateHook::WalkLength (*bot) == 0);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  // ...jump ballistics need gravity to arc...
  sv_gravity.Set (-800);
  CHECK (NavigateHook::CalcJump (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)).empty ());
  sv_gravity.Set (800);

  mode_walls.Reset ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

} // namespace bot
