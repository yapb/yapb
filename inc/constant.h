//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// counter-strike team id's (internal for bot)
namespace bot {

enum class Team : int32_t {
  Terrorist = 0,
  CT,
  Spectator,
  Unassigned,
  Invalid = -1,
  Num = Spectator,
};
YSTL_ENABLE_ENUM_ARITHMETIC (Team);

// counter-strike team id's (used by gamedll, for reference)
enum class CSTeam : int32_t {
  Unassigned = 0,
  Terrorist,
  CT,
  Spectator,
  Any = 5,
  Invalid = -1,
};

// personalities defines
enum class Personality : int32_t {
  Normal = 0,
  Rusher,
  Careful,
  Invalid = -1
};

// bot difficulties
enum class Difficulty : int32_t {
  Noob,
  Easy,
  Normal,
  Hard,
  Expert,
  Invalid = -1
};
YSTL_ENABLE_ENUM_ARITHMETIC (Difficulty);

constexpr auto kInfiniteDistance = 9999999.0f;
constexpr auto kInvalidLightLevel = kInfiniteDistance;
constexpr float kPlayerJumpTakeoff = 268.0f; // engine jump takeoff speed, ~45u height at 800 gravity

// minimal configuration file version accepted by the bot (older configs are rejected before parsing)
constexpr auto kMinimalConfigVersionMajor = 4;
constexpr auto kMinimalConfigVersionMinor = 7;
constexpr auto kBotThinkInterval = 1.0f / 40.0f;
constexpr auto kBotFullThinkInterval = 1.0f / 10.0f;
constexpr auto kGrenadeCheckTime = 0.6f;
constexpr auto kSprayDistance = 272.0f;
constexpr auto kSprayDistanceX2 = kSprayDistance * 2;
constexpr auto kMaxChatterRepeatInterval = 99.0f;
constexpr auto kGrenadeDamageRadius = 385.0f;
constexpr auto kViewFrameUpdate = 1.0f / 25.0f;
constexpr auto kMinMovedDistance = ystl::sqrf (2.0f);
constexpr auto kStuckMaxDuration = 0.75f;
constexpr auto kStuckLatchDuration = 0.5f;
constexpr auto kStuckMoveThreshold = ystl::sqrf (12.0f);
constexpr auto kInfiniteDistanceLong = static_cast<int> (kInfiniteDistance);
constexpr auto kMaxWeapons = 32;
constexpr auto kNumWeapons = 26;
constexpr auto kNumCollisionResponses = 4;
constexpr auto kGameMaxPlayers = 32;
constexpr auto kInvalidNodeIndex = -1;
constexpr auto kConfigExtension = "cfg";
constexpr auto kBombHearDistance = 1536.0f;
constexpr auto kBombPanicHearDistance = 2048.0f;
constexpr auto kBombSiteGoalRadius = 1024.0f; // goals this close to a checked one belong to the same site

} // namespace bot
