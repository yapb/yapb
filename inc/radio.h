//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// radio and chatter messages
namespace bot {

enum class RadioChat : int32_t {
  Invalid = -1,
  InvalidSelect = 0,

  // radio commands (1-34)
  CoverMe = 1,
  YouTakeThePoint = 2,
  HoldThisPosition = 3,
  RegroupTeam = 4,
  FollowMe = 5,
  TakingFireNeedAssistance = 6,
  GoGoGo = 11,
  TeamFallback = 12,
  StickTogetherTeam = 13,
  GetInPositionAndWaitForGo = 14,
  StormTheFront = 15,
  ReportInTeam = 16,
  RogerThat = 21,
  EnemySpotted = 22,
  NeedBackup = 23,
  SectorClear = 24,
  ImInPosition = 25,
  ReportingIn = 26,
  ShesGonnaBlow = 27,
  Negative = 28,
  EnemyDown = 29,

  // chatter commands (35+)
  SpotTheBomber = 34,
  FriendlyFire = 35,
  DiePain = 36,
  Blind = 37,
  GoingToPlantBomb = 38,
  RescuingHostages = 39,
  GoingToCamp = 40,
  TeamAttack = 41,
  TeamKill = 42,
  GuardingPlantedC4 = 43,

  Camping = 44,
  PlantingBomb = 45,
  DefusingBomb = 46,
  InCombat = 47,
  SeekingEnemies = 48,
  Nothing = 49,
  UsingHostages = 50,
  FoundC4 = 51,
  WonTheRound = 52,
  ScaredEmotion = 53,
  HeardTheEnemy = 54,
  SpottedOneEnemy = 55,
  SpottedTwoEnemies = 56,
  SpottedThreeEnemies = 57,

  TooManyEnemies = 58,
  SniperWarning = 59,
  SniperKilled = 60,
  VIPSpotted = 61,
  GuardingEscapeZone = 62,
  GuardingVIPSafety = 63,

  GoingToGuardEscapeZone = 64,
  GoingToGuardRescueZone = 65,
  GoingToGuardVIPSafety = 66,
  QuickWonRound = 67,
  OneEnemyLeft = 68,
  TwoEnemiesLeft = 69,
  ThreeEnemiesLeft = 70,
  NoEnemiesLeft = 71,
  FoundC4Plant = 72,

  WhereIsTheC4 = 73,
  DefendingBombsite = 74,
  BarelyDefused = 75,
  NiceShotCommander = 76,
  NiceShotPall = 77,
  GoingToGuardHostages = 78,
  GoingToGuardDroppedC4 = 79,
  OnMyWay = 80,
  LeadOnSir = 81,
  PinnedDown = 82,
  GottaFindC4 = 83,
  YouHeardTheMan = 84,
  LostCommander = 85,
  NewRound = 86,
  BehindSmoke = 87,
  BombsiteSecured = 88,
  OnARoll = 89,

  Count
};
YSTL_ENABLE_ENUM_ARITHMETIC (RadioChat);

// bot radio and chatter data mixin
class RadioData {
  friend struct TestHook;
  friend struct RadioHook;
  friend struct BehaviorHook;

protected:
  RadioChat radio_select_ {}; // radio entry
  int radio_percent_ {}; // radio usage percent (in response)
  int voice_pitch_ {}; // bot voice pitch

  ystl::FixedArray<float, ystl::to_underlying (RadioChat::Count)> chatter_times_ {}; // chatter command timers
  bool force_radio_ {}; // should bot use radio anyway?

public:
  RadioChat radio_order_ {}; // actual command
  ystl::CountdownTimer team_order_timer_ {}; // time of last radio command
  edict_t *radio_entity_ {}; // pointer to entity issuing a radio command
};

} // namespace bot
