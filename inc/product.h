//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#ifdef VERSION_GENERATED
#  define VERSION_HEADER <version.build.h>
#else
#  define VERSION_HEADER <version.h>
#endif

#include VERSION_HEADER

// simple class for bot internal information
class Product final : public Singleton <Product> {
public:
   struct Build {
      StringRef hash { MODULE_COMMIT_COUNT };
      StringRef count { MODULE_COMMIT_HASH };
      StringRef author { MODULE_AUTHOR };
      StringRef machine { MODULE_MACHINE };
      StringRef compiler { MODULE_COMPILER };
      StringRef id { MODULE_BUILD_ID };
   } build {};

public:
   StringRef name { "YaPB" };
   StringRef year { &__DATE__[7] };
   StringRef author { "YaPB Project" };
   StringRef email { "yapb@jeefo.net" };
   StringRef url { "https://yapb.jeefo.net/" };
   StringRef download { "yapb.jeefo.net" };
   StringRef folder { "yapb" };
   StringRef logtag { "YB" };
   StringRef dtime { __DATE__ " " __TIME__ };
   StringRef date { __DATE__ };
   StringRef version { MODULE_VERSION "." MODULE_COMMIT_COUNT };
   StringRef cmdPri { "yb" };
   StringRef cmdSec { "yapb" };
};

// expose product info
CR_EXPOSE_GLOBAL_SINGLETON (Product, product);
