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

// based on: https://github.com/jeudesprits/PSWyhash/blob/master/Sources/CWyhash/include/wyhash.h
class Random : public Singleton <Random> {
private:
   uint64 div_ { static_cast <uint64> (1) << 32ull };
   uint64 state_ { static_cast <uint64> (time (nullptr)) };

public:
   Random () = default;
   ~Random () = default;

private:
   uint64 wyrand64 () {
      constexpr uint64 wyp0 = 0xa0761d6478bd642full, wyp1 = 0xe7037ed1a0b428dbull;
      state_ += wyp0;

      return mul (state_ ^ wyp1, state_);
   }

   uint32 wyrand32 () {
      return static_cast <uint32> (wyrand64 ());
   }

private:
   uint64_t rotr (uint64 v, uint32 k) {
      return (v >> k) | (v << (64 - k));
   }

   uint64 mul (uint64 a, uint64 b) {
      uint64 hh = (a >> 32) * (b >> 32);
      uint64 hl = (b >> 32) * static_cast <uint32> (b);

      uint64 lh = static_cast <uint32> (a) * (b >> 32);
      uint64 ll = static_cast <uint64> (static_cast <double> (a) * static_cast <double> (b));

      return rotr (hl, 32) ^ rotr (lh, 32) ^ hh ^ ll;
   }

public:
   template <typename U, typename Void = void> U get (U, U) = delete;

   template <typename Void = void> int32 get (int32 low, int32 high) {
      return static_cast <int32> (wyrand32 () * (static_cast <double> (high) - static_cast <double> (low) + 1.0) / div_ + static_cast <double> (low));
   }

   template <typename Void = void> float get (float low, float high) {
      return static_cast <float> (wyrand32 () * (static_cast <double> (high) - static_cast <double> (low)) / (div_ - 1) + static_cast <double> (low));
   }

public:
   bool chance (int32 limit) {
      return get <int32> (0, 100) < limit;
   }
};

// expose global random generator
CR_EXPOSE_GLOBAL_SINGLETON (Random, rg);

CR_NAMESPACE_END
