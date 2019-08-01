//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <crlib/cr-basic.h>

#if defined (CR_HAS_SSE2)
#  include <immintrin.h>
#endif

CR_NAMESPACE_BEGIN

constexpr float kFloatEpsilon = 0.01f;
constexpr float kFloatEqualEpsilon = 0.001f;
constexpr float kFloatCmpEpsilon = 1.192092896e-07f;

constexpr float kMathPi = 3.141592653589793115997963468544185161590576171875f;
constexpr float kMathPiReciprocal = 1.0f / kMathPi;
constexpr float kMathPiHalf = kMathPi / 2;

constexpr float kDegreeToRadians = kMathPi / 180.0f;
constexpr float kRadiansToDegree = 180.0f / kMathPi;

constexpr bool fzero (const float e) {
   return cr::abs (e) < kFloatEpsilon;
}

constexpr bool fequal (const float a, const float b) {
   return cr:: abs (a - b) < kFloatEqualEpsilon;
}

constexpr float radiansToDegrees (const float r) {
   return r * kRadiansToDegree;
}

constexpr float degreesToRadians (const float d) {
   return d * kDegreeToRadians;
}

constexpr float modAngles (const float a) {
   return 360.0f / 65536.0f * (static_cast <int> (a * (65536.0f / 360.0f)) & 65535);
}

constexpr float normalizeAngles (const float a) {
   return 360.0f / 65536.0f * (static_cast <int> ((a + 180.0f) * (65536.0f / 360.0f)) & 65535) - 180.0f;
}

constexpr float anglesDifference (const float a, const float b) {
   return normalizeAngles (a - b);
}

inline float sinf (const float value) {
   const auto sign = static_cast <int32> (value * kMathPiReciprocal);
   const float calc = (value - static_cast <float> (sign) * kMathPi);

   const float sqr = cr::square (calc);
   const float res = 1.00000000000000000000e+00f + sqr * (-1.66666671633720397949e-01f + sqr * (8.33333376795053482056e-03f + sqr * (-1.98412497411482036114e-04f +
      sqr * (2.75565571428160183132e-06f + sqr * (-2.50368472620721149724e-08f + sqr * (1.58849267073435385100e-10f + sqr * -6.58925550841432672300e-13f))))));

   return (sign & 1) ? -calc * res : value * res;
}

inline float cosf (const float value) {
   const auto sign = static_cast <int32> (value * kMathPiReciprocal);
   const float calc = (value - static_cast <float> (sign) * kMathPi);

   const float sqr = cr::square (calc);
   const float res = sqr * (-5.00000000000000000000e-01f + sqr * (4.16666641831398010254e-02f + sqr * (-1.38888671062886714935e-03f + sqr * (2.48006890615215525031e-05f +
      sqr * (-2.75369927749125054106e-07f + sqr * (2.06207229069832465029e-09f + sqr * -9.77507137733812925262e-12f))))));

   const float f = -1.00000000000000000000e+00f;

   return (sign & 1) ? f - res : -f + res;
}

inline float atanf (const float x) {
   const float sqr = cr::square (x);
   return x * (48.70107004404898384f + sqr * (49.5326263772254345f + sqr * 9.40604244231624f)) / (48.70107004404996166f + sqr * (65.7663163908956299f + sqr * (21.587934067020262f + sqr)));
}

inline float atan2f (const float y, const float x) {
   const float ax = cr::abs (x);
   const float ay = cr::abs (y);

   if (ax < 1e-7f && ay < 1e-7f) {
      return 0.0f;
   }

   if (ax > ay) {
      if (x < 0.0f) {
         if (y >= 0.0f) {
            return atanf (y / x) + kMathPi;
         }
         return atanf (y / x) - kMathPi;
      }
      return atanf (y / x);
   }

   if (y < 0.0f) {
      return atanf (-x / y) - kMathPiHalf;
   }
   return atanf (-x / y) + kMathPiHalf;
}

inline float powf (const float x, const float y) {
   union {
      float d;
      int x;
   } res { x };

   res.x = static_cast <int> (y * (res.x - 1064866805) + 1064866805);
   return res.d;
}

inline float sqrtf (const float value) {
   return powf (value, 0.5f);
}

inline float tanf (const float value) {
   return sinf (value) / cosf (value);
}

constexpr float ceilf (const float x) {
   return static_cast <float> (65536 - static_cast <int> (65536.0f - x));
}

inline void sincosf (const float x, const float y, const float z, float *sines, float *cosines) {
#if defined (CR_HAS_SSE2)
   auto set = _mm_set_ps (x, y, z, 0.0f);

   auto _mm_sin = [] (__m128 rad) -> __m128 {
      static auto pi2 = _mm_set_ps1 (kMathPi * 2);
      static auto rp1 = _mm_set_ps1 (4.0f / kMathPi);
      static auto rp2 = _mm_set_ps1 (-4.0f / (kMathPi * kMathPi));
      static auto val = _mm_cmpnlt_ps (rad, _mm_set_ps1 (kMathPi));
      static auto csi = _mm_castsi128_ps (_mm_set1_epi32 (0x80000000));

      val = _mm_and_ps (val, pi2);
      rad = _mm_sub_ps (rad, val);
      val = _mm_cmpngt_ps (rad, _mm_set_ps1 (-kMathPi));
      val = _mm_and_ps (val, pi2);
      rad = _mm_add_ps (rad, val);
      val = _mm_mul_ps (_mm_andnot_ps (csi, rad), rp2);
      val = _mm_add_ps (val, rp1);

      auto si = _mm_mul_ps (val, rad);

      val = _mm_mul_ps (_mm_andnot_ps (csi, si), si);
      val = _mm_sub_ps (val, si);
      val = _mm_mul_ps (val, _mm_set_ps1 (0.225f));

      return _mm_add_ps (val, si);
   };
   static auto hpi = _mm_set_ps1 (kMathPiHalf);

   auto s = _mm_sin (set);
   auto c = _mm_sin (_mm_add_ps (set, hpi));

   _mm_store_ps (sines, _mm_shuffle_ps (s, s, _MM_SHUFFLE (0, 1, 2, 3)));
   _mm_store_ps (cosines, _mm_shuffle_ps (c, c, _MM_SHUFFLE (0, 1, 2, 3)));
#else
   sines[0] = cr::sinf (x);
   sines[1] = cr::sinf (y);
   sines[2] = cr::sinf (z);

   cosines[0] = cr::cosf (x);
   cosines[1] = cr::cosf (y);
   cosines[2] = cr::cosf (z);
#endif
}

CR_NAMESPACE_END