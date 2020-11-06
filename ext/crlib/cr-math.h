//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-basic.h>

#if defined (CR_HAS_SSE)
#  include <pmmintrin.h>
#endif

#include <math.h>

CR_NAMESPACE_BEGIN

constexpr float kFloatEpsilon = 0.01f;
constexpr float kFloatEqualEpsilon = 0.001f;
constexpr float kFloatCmpEpsilon = 1.192092896e-07f;

constexpr float kMathPi = 3.141592653589793115997963468544185161590576171875f;
constexpr float kMathPiHalf = kMathPi * 0.5f;

constexpr float kDegreeToRadians = kMathPi / 180.0f;
constexpr float kRadiansToDegree = 180.0f / kMathPi;

constexpr bool fzero (const float e) {
   return cr::abs (e) < kFloatEpsilon;
}

constexpr bool fequal (const float a, const float b) {
   return cr:: abs (a - b) < kFloatEqualEpsilon;
}

constexpr float rad2deg (const float r) {
   return r * kRadiansToDegree;
}

constexpr float deg2rad (const float d) {
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
   return ::sinf (value);
}

inline float cosf (const float value) {
   return ::cosf (value);
}

inline float atanf (const float value) {
   return ::atanf (value);
}

inline float atan2f (const float y, const float x) {
   return ::atan2f (y, x);
}

inline float powf (const float x, const float y) {
   return ::powf (x, y);
}

inline float sqrtf (const float value) {
   return ::sqrtf (value);
}

inline float tanf (const float value) {
   return ::tanf (value);
}

inline float ceilf (const float value) {
   return ::ceilf (value);
}

inline void sincosf (const float x, const float y, const float z, float *sines, float *cosines) {
#if defined (CR_HAS_SSE)
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
