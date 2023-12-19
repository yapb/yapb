//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
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
   explicit constexpr Product () = default;
   ~Product () = default;

public:
   struct Build {
      static constexpr StringRef hash { MODULE_COMMIT_COUNT };
      static constexpr StringRef count { MODULE_COMMIT_HASH };
      static constexpr StringRef author { MODULE_AUTHOR };
      static constexpr StringRef machine { MODULE_MACHINE };
      static constexpr StringRef compiler { MODULE_COMPILER };
      static constexpr StringRef id { MODULE_BUILD_ID };
   } build {};

public:
   static constexpr StringRef name { "YaPB" };
   static constexpr StringRef nameLower { "yapb" };
   static constexpr StringRef year { &__DATE__[7] };
   static constexpr StringRef author { "YaPB Project" };
   static constexpr StringRef email { "yapb@jeefo.net" };
   static constexpr StringRef url { "https://yapb.jeefo.net/" };
   static constexpr StringRef download { "yapb.jeefo.net" };
   static constexpr StringRef logtag { "YB" };
   static constexpr StringRef dtime { __DATE__ " " __TIME__ };
   static constexpr StringRef date { __DATE__ };
   static constexpr StringRef version { MODULE_VERSION "." MODULE_COMMIT_COUNT };
   static constexpr StringRef cmdPri { "yb" };
   static constexpr StringRef cmdSec { "yapb" };
};

class Folders final : public Singleton <Folders> {
public:
   explicit constexpr Folders () = default;
   ~Folders () = default;

public:
   static constexpr StringRef config { "conf" };
   static constexpr StringRef data { "data" };
   static constexpr StringRef lang { "lang" };
   static constexpr StringRef logs { "logs" };
   static constexpr StringRef train { "train" };
   static constexpr StringRef graph { "graph" };
   static constexpr StringRef podbot { "pwf" };
   static constexpr StringRef ebot { "ewp" };
};

// expose product info
CR_EXPOSE_GLOBAL_SINGLETON (Product, product);
CR_EXPOSE_GLOBAL_SINGLETON (Folders, folders);
