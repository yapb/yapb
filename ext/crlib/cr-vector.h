//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2021 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-math.h>

CR_NAMESPACE_BEGIN

// small simd operations for 3d vector
#if defined (CR_HAS_SSE)

template <typename T> class CR_ALIGN16 SimdWrap {
private:
   __m128 wrap_dp_sse2 (__m128 v1, __m128 v2) {
      auto mul = _mm_mul_ps (v1, v2);
      auto res = _mm_add_ps (_mm_shuffle_ps (v2, mul, _MM_SHUFFLE (1, 0, 0, 0)), mul);

      mul = _mm_add_ps (_mm_shuffle_ps (mul, res, _MM_SHUFFLE (0, 3, 0, 0)), res);

      return _mm_shuffle_ps (mul, mul, _MM_SHUFFLE (2, 2, 2, 2));
   }

public:
   union {
      __m128 m;

      struct {
         T x, y, z;
      } vec;
   };

   SimdWrap (const T &x, const T &y, const T &z) {
      m = _mm_set_ps (0.0f, z, y, x);
   }

   SimdWrap (const T &x, const T &y) {
      m = _mm_set_ps (0.0f, 0.0f, y, x);
   }

   SimdWrap (__m128 m) : m (m)
   { }

public:
   SimdWrap normalize () {
      return { _mm_div_ps (m, _mm_sqrt_ps (wrap_dp_sse2 (m, m))) };
   }
};

#endif

// 3dmath vector
template <typename T> class Vec3D {
public:
   T x {};
   T y {};
   T z {};

public:
   Vec3D (const T &scaler = 0.0f) : x (scaler), y (scaler), z (scaler)
   { }

   Vec3D (const T &x, const T &y, const T &z) : x (x), y (y), z (z)
   { }

   Vec3D (T *rhs) : x (rhs[0]), y (rhs[1]), z (rhs[2])
   { }

#if defined (CR_HAS_SSE)
   Vec3D (const SimdWrap <T> &rhs) : x (rhs.vec.x), y (rhs.vec.y), z (rhs.vec.z)
   { }
#endif

   Vec3D (const Vec3D &) = default;

   Vec3D (decltype (nullptr)) {
      clear ();
   }

public:
   operator T * () {
      return &x;
   }

   operator const T * () const {
      return &x;
   }

   Vec3D operator + (const Vec3D &rhs) const {
      return { x + rhs.x, y + rhs.y, z + rhs.z };
   }

   Vec3D operator - (const Vec3D &rhs) const {
      return { x - rhs.x, y - rhs.y, z - rhs.z };
   }

   Vec3D operator - () const {
      return { -x, -y, -z };
   }

   friend Vec3D operator * (const T &scale, const Vec3D &rhs) {
      return { rhs.x * scale, rhs.y * scale, rhs.z * scale };
   }

   Vec3D operator * (const T &scale) const {
      return { scale * x, scale * y, scale * z };
   }

   Vec3D operator / (const T &rhs) const {
      const auto inv = 1 / (rhs + kFloatEqualEpsilon);
      return { inv * x, inv * y, inv * z };
   }

   // cross product
   Vec3D operator ^ (const Vec3D &rhs) const {
      return { y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x };
   }

   // dot product
   T operator | (const Vec3D &rhs) const {
      return x * rhs.x + y * rhs.y + z * rhs.z;
   }

   const Vec3D &operator += (const Vec3D &rhs) {
      x += rhs.x;
      y += rhs.y;
      z += rhs.z;

      return *this;
   }

   const Vec3D &operator -= (const Vec3D &rhs) {
      x -= rhs.x;
      y -= rhs.y;
      z -= rhs.z;

      return *this;
   }

   const Vec3D &operator *= (const T &rhs) {
      x *= rhs;
      y *= rhs;
      z *= rhs;

      return *this;
   }

   const Vec3D &operator /= (const T &rhs) {
      const auto inv = 1 / (rhs + kFloatEqualEpsilon);

      x *= inv;
      y *= inv;
      z *= inv;

      return *this;
   }

   bool operator == (const Vec3D &rhs) const {
      return cr::fequal (x, rhs.x) && cr::fequal (y, rhs.y) && cr::fequal (z, rhs.z);
   }

   bool operator != (const Vec3D &rhs) const {
      return !operator == (rhs);
   }

   void operator = (decltype (nullptr)) {
      clear ();
   }

   const float &operator [] (const int i) const {
      return &(x)[i];
   }

   float &operator [] (const int i) {
      return &(x)[i];
   }

   Vec3D &operator = (const Vec3D &) = default;

public:
   T length () const {
      return cr::sqrtf (lengthSq ());
   }

   T length2d () const {
      return cr::sqrtf (cr::square (x) + cr::square (y));
   }

   T lengthSq () const {
      return cr::square (x) + cr::square (y) + cr::square (z);
   }

   Vec3D get2d () const {
      return { x, y, 0.0f };
   }

   Vec3D normalize () const {
#if defined (CR_HAS_SSE)
      return SimdWrap <T> { x, y, z }.normalize ();
#else
      auto len = length () + cr::kFloatCmpEpsilon;

      if (cr::fzero (len)) {
         return { 0.0f, 0.0f, 1.0f };
      }
      len = 1.0f / len;
      return { x * len, y * len, z * len };
#endif
   }

   Vec3D normalize2d () const {
#if defined (CR_HAS_SSE)
      return SimdWrap <T> { x, y }.normalize ();
#else
      auto len = length2d () + cr::kFloatCmpEpsilon;

      if (cr::fzero (len)) {
         return { 0.0f, 1.0f, 0.0f };
      }
      len = 1.0f / len;
      return { x * len, y * len, 0.0f };
#endif
   }

   bool empty () const {
      return cr::fzero (x) && cr::fzero (y) && cr::fzero (z);
   }

   void clear () {
      x = y = z = 0.0f;
   }

   Vec3D clampAngles () {
      x = cr::normalizeAngles (x);
      y = cr::normalizeAngles (y);
      z = 0.0f;

      return *this;
   }

   T pitch () const {
      if (cr::fzero (z)) {
         return 0.0f;
      }
      return cr::deg2rad (cr::atan2f (z, length2d ()));
   }

   T yaw () const {
      if (cr::fzero (x) && cr::fzero (y)) {
         return 0.0f;
      }
      return cr::rad2deg (cr:: atan2f (y, x));
   }

   Vec3D angles () const {
      if (cr::fzero (x) && cr::fzero (y)) {
         return { z > 0.0f ? 90.0f : 270.0f, 0.0, 0.0f };
      }
      return { cr::rad2deg (cr::atan2f (z, length2d ())), cr::rad2deg (cr::atan2f (y, x)), 0.0f };
   }

   void angleVectors (Vec3D *forward, Vec3D *right, Vec3D *upward) const {
      enum { pitch, yaw, roll, unused, max };

      T sines[max] = { 0.0f, 0.0f, 0.0f, 0.0f };
      T cosines[max] = { 0.0f, 0.0f, 0.0f, 0.0f };

      // compute the sine and cosine compontents
      cr::sincosf (cr::deg2rad (x), cr::deg2rad (y), cr::deg2rad (z), sines, cosines);

      if (forward) {
         *forward = {
            cosines[pitch] * cosines[yaw],
            cosines[pitch] * sines[yaw],
            -sines[pitch]
         };
      }

      if (right) {
         *right = {
            -sines[roll] * sines[pitch] * cosines[yaw] + cosines[roll] * sines[yaw],
            -sines[roll] * sines[pitch] * sines[yaw] - cosines[roll] * cosines[yaw],
            -sines[roll] * cosines[pitch]
         };
      }

      if (upward) {
         *upward = {
            cosines[roll] * sines[pitch] * cosines[yaw] + sines[roll] * sines[yaw],
            upward->y = cosines[roll] * sines[pitch] * sines[yaw] - sines[roll] * cosines[yaw],
            upward->z = cosines[roll] * cosines[pitch]
         };
      }
   }

   const Vec3D &forward () {
      static Vec3D s_fwd {};
      angleVectors (&s_fwd, nullptr, nullptr);

      return s_fwd;
   }

   const Vec3D &upward () {
      static Vec3D s_up {};
      angleVectors (nullptr, nullptr, &s_up);

      return s_up;
   }

   const Vec3D &right () {
      static Vec3D s_right {};
      angleVectors (nullptr, &s_right, nullptr);

      return s_right;
   }
};

// default is float
using Vector = Vec3D <float>;

CR_NAMESPACE_END
