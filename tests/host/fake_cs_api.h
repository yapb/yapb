//
// YaPB test host: fake cs gamedll observation API.
//
// SPDX-License-Identifier: Unlicense
//
// Implemented by fake_cs/fake_cs.cpp, consumed by the host through the
// platform loader (yapb loads the fake gamedll with local visibility, so
// plain linking against it is impossible by design).
//

#pragma once

#if defined(__cplusplus)
namespace bot {

extern "C" {
#endif

// number of recorded baseline gamedll calls
int FakeCsCallCount ();

// recorded call description by index: "FuncName detail"
const char *FakeCsCallAt (int index);

// clear the call log
void FakeCsReset ();

// 1 when the gamedll entry points were reached at all
int FakeCsEntered ();

#if defined(__cplusplus)
} // extern "C"
#endif

} // namespace bot
