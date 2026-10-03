//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// the gravity yb_fun_mode imonmars sets on the server
namespace bot {

constexpr float kFunMarsGravity = 325.0f;

// classic podbot fun modes
enum class FunModeId : int32_t {
  Off = 0, // imsober
  Tron = 1, // tronisback
  NewYear = 2, // itsnewyear
  Haunted = 3, // imhaunted
  Dark = 4, // itstoodark
  Stoned = 5, // stonedagain
  Mars = 6, // imonmars
  Invalid = -1
};

// fun mode manager
class FunMode final : public ystl::Singleton<FunMode> {
private:
  FunModeId last_mode_ {}; // last processed mode, to detect out of band cvar changes
  float saved_gravity_ {}; // sv_gravity value prior to fun mode activation
  ystl::CountdownTimer shake_timer_ {}; // stoned mode shake interval

public:
  FunMode ();
  ~FunMode () = default;

private:
  // light-cycle glow for everyone, red terrorists, blue cts
  void ApplyTron ();

  // firework sparks around the bots eyes
  void ApplyNewYear ();

  // bots are ghosts now
  void ApplyHaunted ();

  // bots will light your way
  void ApplyDark ();

  // periodic screen shake for everyone
  void ApplyStoned ();

  // keeps the gravity low
  void ApplyMars ();

  // reverts all the things mode could touch
  void TurnOff ();

  // one-time actions upon mode activation
  void OnModeChanged ();

public:
  // parses the mode from keyword
  static FunModeId ParseMode (ystl::StringRef value);

  // gets the keyword for mode
  static ystl::StringRef KeywordOf (FunModeId mode);

  // gets the classic message for mode
  static ystl::StringRef MessageOf (FunModeId mode);

public:
  // per-frame update, detects transitions and applies mode effects
  void Update ();

  // sets the mode, persists via cvar
  void SetMode (FunModeId mode);

  // gets the currently active mode
  FunModeId Mode () const {
    return last_mode_;
  }

  // reset timers upon level change, as game time is about to be reset
  void ResetTimers () {
    shake_timer_.invalidate ();
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (FunMode, fun_mode);

} // namespace bot
