//
// YaPB test host: shared scenario helpers.
//
// SPDX-License-Identifier: Unlicense
//
// FakeCS resolution via the platform loader (yapb loads the fake gamedll
// with local visibility, so plain linking against it is impossible by design).
// ystl only, no std::*.
//

#pragma once

#include <cstring>

#include <ystl/ystl.h>
#include <linkage/goldsrc.h>

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "fake_cs_api.h"

#if defined(YSTL_WINDOWS)
  // windows.h defines these as empty legacy macros; the harness (and the
  // scenarios) use them as ordinary identifiers
  #undef near
  #undef far
#endif

// yapb entry points (linked from yapb_static, extern "C" via YSTL_EXPORT)
namespace bot {

extern "C" {
void GiveFnptrsToDll (enginefuncs_t *funcs, globalvars_t *globals);
int GetEntityAPI (gamefuncs_t *table, int version);
int GetNewDLLFunctions (newgamefuncs_t *table, int *version);
IBotModule *GetBotAPI (int version);
}

// ystl REQUIRE only records, it never aborts: hard prerequisites return early.
// single evaluation: actions (advance, sync) must never run twice.
#define HOST_REQUIRE(expr) do { const bool _hostOk = !!(expr); REQUIRE (_hostOk); if (!_hostOk) { return; } } while (0)

namespace testhost {

#if defined(YSTL_WINDOWS)
using ModuleHandle = HMODULE;
#else
using ModuleHandle = void *;
#endif

// portable dynamic loading: tests only (yapb itself goes through ystl)
inline ModuleHandle ModuleOpen (const char *path) {
#if defined(YSTL_WINDOWS)
  return LoadLibraryA (path);
#else
  // RTLD_GLOBAL: the same image is preloaded for yapb's own load, and
  // symbols must stay visible to the process later on
  return dlopen (path, RTLD_NOW | RTLD_GLOBAL);
#endif
}

inline void *ModuleSymbol (ModuleHandle handle, const char *name) {
#if defined(YSTL_WINDOWS)
  return reinterpret_cast<void *> (GetProcAddress (handle, name));
#else
  return dlsym (handle, name);
#endif
}

inline void ModuleClose (ModuleHandle handle) {
  if (!handle) {
    return;
  }
#if defined(YSTL_WINDOWS)
  FreeLibrary (handle);
#else
  dlclose (handle);
#endif
}

struct FakeCSApi {
  int (*call_count) () = nullptr;
  const char *(*call_at) (int) = nullptr;
  void (*reset) () = nullptr;
  int (*entered) () = nullptr;
  ModuleHandle handle = nullptr;

  bool Valid () const {
    return call_count && call_at && reset && entered && handle;
  }

  bool HasCall (const char *prefix) const {
    return CallIndex (prefix) >= 0;
  }

  // index of the first recorded call with the prefix, -1 when absent
  int CallIndex (const char *prefix) const {
    const size_t len = strlen (prefix);

    for (int i = 0; i < call_count (); ++i) {
      if (strncmp (call_at (i), prefix, len) == 0) {
        return i;
      }
    }
    return -1;
  }
};

inline bool ResolveFakeCs (const char *path, FakeCSApi &api) {
  const auto handle = ModuleOpen (path);

  if (!handle) {
    return false;
  }
  api.handle = handle;
  api.reset = reinterpret_cast<void (*) ()> (ModuleSymbol (handle, "FakeCS_Reset"));
  api.call_count = reinterpret_cast<int (*) ()> (ModuleSymbol (handle, "FakeCS_CallCount"));
  api.call_at = reinterpret_cast<const char *(*)(int)> (ModuleSymbol (handle, "FakeCS_CallAt"));
  api.entered = reinterpret_cast<int (*) ()> (ModuleSymbol (handle, "FakeCS_Entered"));

  return api.Valid ();
}

inline void CloseFakeCs (FakeCSApi &api) {
  if (api.handle) {
    ModuleClose (api.handle);
    api.handle = nullptr;
  }
}

// lightest hermetic prefix: engine tables only, no postload/cs load/lifecycle.
// enough for graph math, traces, messages, cvar reads and timers
// (levelInitialize does the same time-source binding on live servers).
inline void InstallEngineTables (FakeEngine &engine) {
  memcpy (&engfuncs, &engine.Funcs (), sizeof (enginefuncs_t));
  globals = &engine.Globals ();
  ystl::timer_source.set_time_address (&engine.Globals ().time);
}

// one queued manual request per maintainQuota call (0.1s gate): pump until
// count is reached, fail the test on timeout instead of hanging
inline bool PumpBots (FakeEngine &engine, int want, int max_iters = 40) {
  for (int i = 0; i < max_iters && bots.GetBotCount () != want; ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();
  }
  return bots.GetBotCount () == want;
}

// first bot whose netname matches, null when absent
inline Bot *FindBot (ystl::StringRef name) {
  Bot *found = nullptr;

  bots.ForEach ([&] (Bot *bot) {
    if (ystl::StringRef (bot->pev->netname.chars ()) == name) {
      found = bot;
      return true;
    }
    return false;
  });
  return found;
}

// exact fake-CS call count by name (buy;menuselect is 2+1)
inline int CsCalls (FakeCSApi &cs, const char *name) {
  int count = 0;

  for (int i = 0; i < cs.call_count (); ++i) {
    if (ystl::StringRef (cs.call_at (i)) == name) {
      ++count;
    }
  }
  return count;
}

} // namespace testhost

} // namespace bot
