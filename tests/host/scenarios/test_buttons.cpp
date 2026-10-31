//
// YaPB test host: unit/buttons_lifecycle.
//
// SPDX-License-Identifier: Unlicense
//
// Covers the InputButtons hold model: untimed taps, timed holds, explicit
// release, untimed-over-timed override, independent masks and ReleaseAll.
//

#include <yapb.h>

#include <ystl/test.h>

namespace bot {

TEST_CASE ("unit/buttons_lifecycle") {
  float now = 0.0f;
  ystl::timer_source.set_time_address (&now);

  int32_t bits = 0;
  InputButtons buttons;
  buttons.Bind (&bits);

  // untimed press is a tap that dies on the next frame boundary
  buttons.PressDuck ();
  CHECK (buttons.IsDucking ());
  CHECK ((bits & IN_DUCK) != 0);
  buttons.BeginFrame ();
  CHECK (!buttons.IsDucking ());
  CHECK (bits == 0);

  // timed press survives frame boundaries until the hold expires
  buttons.PressAttack (5.0f);
  CHECK (buttons.IsAttacking ());
  buttons.BeginFrame ();
  CHECK (buttons.IsAttacking ());
  now += 2.0f;
  buttons.BeginFrame ();
  CHECK (buttons.IsAttacking ());
  now += 4.0f;
  buttons.BeginFrame ();
  CHECK (!buttons.IsAttacking ());

  // explicit release wins over an active hold
  buttons.PressJump (5.0f);
  buttons.ReleaseJump ();
  CHECK (!buttons.IsJumping ());
  now += 1.0f;
  buttons.BeginFrame ();
  CHECK (!buttons.IsJumping ());

  // a later untimed press drops the old timer, so it is not held past its frame
  buttons.PressDuck (5.0f);
  now += 0.1f;
  buttons.PressDuck ();
  buttons.BeginFrame ();
  CHECK (!buttons.IsDucking ());

  // a timed press upgrades an untimed one and keeps holding across frames
  buttons.PressDuck ();
  buttons.PressDuck (5.0f);
  buttons.BeginFrame ();
  CHECK (buttons.IsDucking ());
  now += 2.0f;
  buttons.BeginFrame ();
  CHECK (buttons.IsDucking ());
  now += 4.0f;
  buttons.BeginFrame ();
  CHECK (!buttons.IsDucking ());

  // ReleaseAll also drops active timed holds, they must not reappear later
  buttons.PressAttack (5.0f);
  buttons.ReleaseAll ();
  CHECK (!buttons.IsAttacking ());
  now += 10.0f;
  buttons.BeginFrame ();
  CHECK (!buttons.IsAttacking ());

  // release after press wins, and press after release wins
  buttons.PressAttack ();
  buttons.ReleaseAttack ();
  CHECK (!buttons.IsAttacking ());
  buttons.ReleaseAttack ();
  buttons.PressAttack ();
  CHECK (buttons.IsAttacking ());
  buttons.BeginFrame ();
  CHECK (!buttons.IsAttacking ());

  // repeated frame boundaries keep a live timed hold alive
  buttons.PressDuck (5.0f);
  buttons.BeginFrame ();
  buttons.BeginFrame ();
  CHECK (buttons.IsDucking ());
  buttons.ReleaseAll ();

  // buttons are independent, ReleaseMask only drops the requested bits
  buttons.PressDuck (5.0f);
  buttons.PressJump (5.0f);
  buttons.ReleaseDuck ();
  buttons.BeginFrame ();
  CHECK (!buttons.IsDucking ());
  CHECK (buttons.IsJumping ());

  // ReleaseAll clears both the mirror and every hold
  buttons.ReleaseAll ();
  CHECK (bits == 0);
  CHECK (!buttons.IsJumping ());
  buttons.BeginFrame ();
  CHECK (bits == 0);

  // combined movement helpers track real bits
  buttons.PressForward ();
  buttons.PressMoveLeft ();
  CHECK (buttons.IsMovingForward ());
  CHECK (buttons.IsStrafing ());
  buttons.ReleaseMove ();
  CHECK (bits == 0);
}

} // namespace bot
