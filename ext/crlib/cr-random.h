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

#include <time.h>
#include <crlib/cr-basic.h>

CR_NAMESPACE_BEGIN

// random number generator see: https://github.com/preshing/RandomSequence/
class Random final : public Singleton <Random> {
private:
   uint32 index_, offset_;
   uint64 divider_;

public:
   explicit Random () {
      const auto base = static_cast <uint32> (time (nullptr));
      const auto offset = base + 1;

      index_ = premute (premute (base) + 0x682f0161);
      offset_ = premute (premute (offset) + 0x46790905);
      divider_ = (static_cast <uint64> (1)) << 32;
   }
   ~Random () = default;

private:
   uint32 premute (uint32 index) {
      static constexpr auto prime = 4294967291u;

      if (index >= prime) {
         return index;
      }
      const uint32 residue = (static_cast <uint64> (index) * index) % prime;
      return (index <= prime / 2) ? residue : prime - residue;
   }

   uint32 generate () {
      return premute ((premute (index_++) + offset_) ^ 0x5bf03635);
   }

public:
   template <typename U> U int_ (U low, U high) {
      return static_cast <U> (generate () * (static_cast <double> (high) - static_cast <double> (low) + 1.0) / divider_ + static_cast <double> (low));
   }

   float float_ (float low, float high) {
      return static_cast <float> (generate () * (static_cast <double> (high) - static_cast <double> (low)) / (divider_ - 1) + static_cast <double> (low));
   }

   template <typename U> bool chance (const U max, const U maxChance = 100) {
      return int_ <U> (0, maxChance) < max;
   }
};

// expose global random generator
CR_EXPOSE_GLOBAL_SINGLETON (Random, rg);

CR_NAMESPACE_END
