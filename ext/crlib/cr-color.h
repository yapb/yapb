//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-basic.h>

CR_NAMESPACE_BEGIN

// simple color holder
class Color final {
public:
   int32 red = 0, green = 0, blue = 0;

public:
   Color (int32 r, int32 g, int32 b) : red (r), green (g), blue (b) { }

   explicit Color () = default;
   ~Color () = default;

public:
   void reset () {
      red = green = blue = 0;
   }

   int32 avg () const {
      return sum () / (sizeof (Color) / sizeof (int32));
   }

   int32 sum () const {
      return red + green + blue;
   }
};

CR_NAMESPACE_END
