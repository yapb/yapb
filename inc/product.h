//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

#ifdef VERSION_GENERATED
  #define VERSION_HEADER <version.build.h>
#else
  #define VERSION_HEADER <version.h>
#endif

#include VERSION_HEADER

// compile time build string
#define CTS_BUILD_STR static inline constexpr ystl::StringRef

// simple class for bot internal information
namespace bot {

static constexpr class Product final {
public:
  explicit constexpr Product () = default;
  ~Product () = default;

public:
  static constexpr struct BuildInfo {
    CTS_BUILD_STR hash { MODULE_COMMIT_HASH };
    CTS_BUILD_STR count { MODULE_COMMIT_COUNT };
    CTS_BUILD_STR author { MODULE_AUTHOR };
    CTS_BUILD_STR machine { MODULE_MACHINE };
    CTS_BUILD_STR compiler { MODULE_COMPILER };
    CTS_BUILD_STR id { MODULE_BUILD_ID };
  } bi {};

public:
  CTS_BUILD_STR name { "YaPB" };
  CTS_BUILD_STR name_lower { "yapb" };
  CTS_BUILD_STR year { MODULE_BUILD_YEAR };
  CTS_BUILD_STR author { "YaPB Team" };
  CTS_BUILD_STR email { "yapb@jeefo.net" };
  CTS_BUILD_STR url { "https://yapb.github.io/" };
  CTS_BUILD_STR download { "https://raw.githubusercontent.com/yapb/graph/refs/heads/master" };
  CTS_BUILD_STR upload { "https://graph-worker.yapb.workers.dev" };
  CTS_BUILD_STR logtag { "YB" };
  CTS_BUILD_STR dtime { MODULE_BUILD_DATE " " MODULE_BUILD_TIME };
  CTS_BUILD_STR date { MODULE_BUILD_DATE };
  CTS_BUILD_STR version { MODULE_VERSION "." MODULE_COMMIT_COUNT };
  CTS_BUILD_STR cmd_pri { "yb" };
  CTS_BUILD_STR cmd_sec { "yapb" };
} product {};

static constexpr class Folders final {
public:
  explicit constexpr Folders () = default;
  ~Folders () = default;

public:
  CTS_BUILD_STR bot { product.name_lower };
  CTS_BUILD_STR addons { "addons" };
  CTS_BUILD_STR config { "conf" };
  CTS_BUILD_STR extra { "extra" };
  CTS_BUILD_STR data { "data" };
  CTS_BUILD_STR lang { "lang" };
  CTS_BUILD_STR logs { "logs" };
  CTS_BUILD_STR train { "train" };
  CTS_BUILD_STR graph { "graph" };
  CTS_BUILD_STR podbot { "pwf" };
  CTS_BUILD_STR bin { "bin" };
} folders {};

#undef CTS_BUILD_STR

} // namespace bot
