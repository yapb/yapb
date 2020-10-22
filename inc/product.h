//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Development Team <team@yapb.ru>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
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
      StringRef hash { MODULE_BUILD_HASH };
      StringRef author { MODULE_BUILD_AUTHOR };
      StringRef count { MODULE_BUILD_COUNT };
      StringRef machine { MODULE_BUILD_MACHINE };
      StringRef compiler { MODULE_BUILD_COMPILER };
      StringRef id { MODULE_BOT_BUILD_ID };
   } build { };

public:
   StringRef name { "YaPB" };
   StringRef year { __DATE__ + 7 };
   StringRef author { "YaPB Development Team" };
   StringRef email { "team@yapb.ru" };
   StringRef url { "https://yapb.ru/" };
   StringRef download { "yapb.ru" };
   StringRef folder { "yapb" };
   StringRef logtag { "YB" };
   StringRef dtime { __DATE__ " " __TIME__ };
   StringRef date { __DATE__ };
   StringRef version { MODULE_BOT_VERSION "." MODULE_BUILD_COUNT };
   StringRef cmdPri { "yb" };
   StringRef cmdSec { "yapb" };
};

// expose product info
CR_EXPOSE_GLOBAL_SINGLETON (Product, product);
