//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
//

#pragma once

#include <time.h>
#include <crlib/cr-basic.h>

CR_NAMESPACE_BEGIN

// random number generator see: https://github.com/preshing/RandomSequence/
class Random final : public Singleton <Random> {
private:
   uint32 m_index, m_offset;
   uint64 m_divider;

public:
   explicit Random () {
      const auto base = static_cast <uint32> (time (nullptr));
      const auto offset = base + 1;

      m_index = premute (premute (base) + 0x682f0161);
      m_offset = premute (premute (offset) + 0x46790905);
      m_divider = (static_cast <uint64> (1)) << 32;
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
      return premute ((premute (m_index++) + m_offset) ^ 0x5bf03635);
   }

public:
   template <typename U> U int_ (U low, U high) {
      return static_cast <U> (generate () * (static_cast <double> (high) - static_cast <double> (low) + 1.0) / m_divider + static_cast <double> (low));
   }

   float float_ (float low, float high) {
      return static_cast <float> (generate () * (static_cast <double> (high) - static_cast <double> (low)) / (m_divider - 1) + static_cast <double> (low));
   }

   template <typename U> bool chance (const U max, const U maxChance = 100) {
      return int_ <U> (0, maxChance) < max;
   }
};

// expose global random generator
static auto &rg = Random::get ();

CR_NAMESPACE_END