//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
//

#pragma once

#include <crlib/cr-math.h>

CR_NAMESPACE_BEGIN

// 3dmath vector
template <typename T> class Vec3D {
public:
   T x = 0.0f, y = 0.0f, z = 0.0f;

public:
   Vec3D (const T &scaler = 0.0f) : x (scaler), y (scaler), z (scaler)
   { }

   Vec3D (const T &x, const T &y, const T &z) : x (x), y (y), z (z)
   { }

   Vec3D (T *rhs) : x (rhs[0]), y (rhs[1]), z (rhs[2])
   { }

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

   Vec3D &operator = (const Vec3D &) = default;

public:
   T length () const {
      return cr::sqrtf (lengthSq ());
   }

   T length2d () const {
      return cr::sqrtf (x * x + y * y);
   }

   T lengthSq () const {
      return x * x + y * y + z * z;
   }

   Vec3D get2d () const {
      return { x, y, 0.0f };
   }

   Vec3D normalize () const {
      auto len = length () + cr::kFloatCmpEpsilon;

      if (cr::fzero (len)) {
         return { 0.0f, 0.0f, 1.0f };
      }
      len = 1.0f / len;
      return { x * len, y * len, z * len };
   }

   Vec3D normalize2d () const {
      auto len = length2d () + cr::kFloatCmpEpsilon;

      if (cr::fzero (len)) {
         return { 0.0f, 1.0f, 0.0f };
      }
      len = 1.0f / len;
      return { x * len, y * len, 0.0f };
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
      if (cr::fzero (x) && cr::fzero (y)) {
         return 0.0f;
      }
      return cr::degreesToRadians (cr::atan2f (z, length2d ()));
   }

   T yaw () const {
      if (cr::fzero (x) && cr::fzero (y)) {
         return 0.0f;
      }
      return cr::radiansToDegrees (cr:: atan2f (y, x));
   }

   Vec3D angles () const {
      if (cr::fzero (x) && cr::fzero (y)) {
         return { z > 0.0f ? 90.0f : 270.0f, 0.0, 0.0f };
      }
      return { cr::radiansToDegrees (cr::atan2f (z, length2d ())), cr::radiansToDegrees (cr::atan2f (y, x)), 0.0f };
   }

   void angleVectors (Vec3D *forward, Vec3D *right, Vec3D *upward) const {
      enum { pitch, yaw, roll, unused, max };

      T sines[max] = { 0.0f, 0.0f, 0.0f, 0.0f };
      T cosines[max] = { 0.0f, 0.0f, 0.0f, 0.0f };

      // compute the sine and cosine compontents
      cr::sincosf (cr::degreesToRadians (x), cr::degreesToRadians (y), cr::degreesToRadians (z), sines, cosines);

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