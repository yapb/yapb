//
// YaPB test host entry point (ystl framework runner).
//
// SPDX-License-Identifier: Unlicense
//

#include <ystl/test.h>

thread_local SectionTracker g_sections;
thread_local TestResults g_results;

int main (int argc, char **argv) {
#if defined(YSTL_WINDOWS)
  // prod aborts when HTTP is used before Winsock is up; mark sockets started
  // without spawning the connectivity probe (an empty host keeps it
  // synchronous). Test builds have no TLS, so https graph lookups short
  // circuit as HttpOnly and never touch the network.
  ystl::http.startup ();
#endif
  // ctest sets this; default it for manual runs so the bot worker pool does
  // not alter timing-sensitive scenarios
  if (ystl::plat.env ("YB_SINGLE_THREADED")[0] == '\0') {
#if defined(YSTL_WINDOWS)
    _putenv_s ("YB_SINGLE_THREADED", "1");
#else
    setenv ("YB_SINGLE_THREADED", "1", 1);
#endif
  }

  // bot singletons cannot be reset between cases: transparently re-exec one
  // case per process when more than one is selected
  ystl::test::set_fork_per_case (true);

  return ystl::test::run_main (argc, argv);
}
