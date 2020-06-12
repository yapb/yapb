//
// CRLib - Simple library for STL replacement in private projects.
// Copyright © 2020 YaPB Development Team <team@yapb.ru>.
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
