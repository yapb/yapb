//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <crlib/cr-math.h>

CR_NAMESPACE_BEGIN

// 3dmath vector
class Vector final {
public:
   float x = 0.0f, y = 0.0f, z = 0.0f;

public:
   Vector (const float scaler = 0.0f) : x (scaler), y (scaler), z (scaler)
   { }

   explicit Vector (const float _x, const float _y, const float _z) : x (_x), y (_y), z (_z)
   { }

   Vector (float *rhs) : x (rhs[0]), y (rhs[1]), z (rhs[2])
   { }

   Vector (const Vector &) = default;

public:
   operator float *() {
      return &x;
   }

   operator const float * () const {
      return &x;
   }

   Vector operator + (const Vector &rhs) const {
      return Vector (x + rhs.x, y + rhs.y, z + rhs.z);
   }

   Vector operator - (const Vector &rhs) const {
      return Vector (x - rhs.x, y - rhs.y, z - rhs.z);
   }

   Vector operator - () const {
      return Vector (-x, -y, -z);
   }

   friend Vector operator * (const float scale, const Vector &rhs) {
      return Vector (rhs.x * scale, rhs.y * scale, rhs.z * scale);
   }

   Vector operator * (const float scale) const {
      return Vector (scale * x, scale * y, scale * z);
   }

   Vector operator / (const float div) const {
      const float inv = 1 / div;
      return Vector (inv * x, inv * y, inv * z);
   }

   // cross product
   Vector operator ^ (const Vector &rhs) const {
      return Vector (y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
   }

   // dot product
   float operator | (const Vector &rhs) const {
      return x * rhs.x + y * rhs.y + z * rhs.z;
   }

   const Vector &operator += (const Vector &rhs) {
      x += rhs.x;
      y += rhs.y;
      z += rhs.z;

      return *this;
   }

   const Vector &operator -= (const Vector &right) {
      x -= right.x;
      y -= right.y;
      z -= right.z;

      return *this;
   }

   const Vector &operator *= (float scale) {
      x *= scale;
      y *= scale;
      z *= scale;

      return *this;
   }

   const Vector &operator /= (float div) {
      const float inv = 1 / div;

      x *= inv;
      y *= inv;
      z *= inv;

      return *this;
   }

   bool operator == (const Vector &rhs) const {
      return cr::fequal (x, rhs.x) && cr::fequal (y, rhs.y) && cr::fequal (z, rhs.z);
   }

   bool operator != (const Vector &rhs) const {
      return !cr::fequal (x, rhs.x) && !cr::fequal (y, rhs.y) && !cr::fequal (z, rhs.z);
   }

   Vector &operator = (const Vector &) = default;

public:
   float length () const {
      return cr::sqrtf (lengthSq ());
   }

   float length2d () const {
      return cr::sqrtf (x * x + y * y);
   }

   float lengthSq () const {
      return x * x + y * y + z * z;
   }

   Vector get2d () const {
      return Vector (x, y, 0.0f);
   }

   Vector normalize () const {
      float len = length () + cr::kFloatCmpEpsilon;

      if (cr::fzero (len)) {
         return Vector (0.0f, 0.0f, 1.0f);
      }
      len = 1.0f / len;
      return Vector (x * len, y * len, z * len);
   }

   Vector normalize2d () const {
      float len = length2d () + cr::kFloatCmpEpsilon;

      if (cr::fzero (len)) {
         return Vector (0.0f, 1.0f, 0.0f);
      }
      len = 1.0f / len;
      return Vector (x * len, y * len, 0.0f);
   }

   bool empty () const {
      return cr::fzero (x) && cr::fzero (y) && cr::fzero (z);
   }

   static const Vector &null () {
      static const Vector &s_null {};
      return s_null;
   }

   void clear () {
      x = y = z = 0.0f;
   }

   Vector clampAngles () {
      x = cr::normalizeAngles (x);
      y = cr::normalizeAngles (y);
      z = 0.0f;

      return *this;
   }

   float pitch () const {
      if (cr::fzero (x) && cr::fzero (y)) {
         return 0.0f;
      }
      return cr::degreesToRadians (cr::atan2f (z, length2d ()));
   }

   float yaw () const {
      if (cr::fzero (x) && cr::fzero (y)) {
         return 0.0f;
      }
      return cr::radiansToDegrees (cr:: atan2f (y, x));
   }

   Vector angles () const {
      if (cr::fzero (x) && cr::fzero (y)) {
         return Vector (z > 0.0f ? 90.0f : 270.0f, 0.0, 0.0f);
      }
      return Vector (cr::radiansToDegrees (cr::atan2f (z, length2d ())), cr::radiansToDegrees (cr::atan2f (y, x)), 0.0f);
   }

   void buildVectors (Vector *forward, Vector *right, Vector *upward) const {
      enum { pitch, yaw, roll, unused, max };

      float sines[max] = { 0.0f, 0.0f, 0.0f, 0.0f };
      float cosines[max] = { 0.0f, 0.0f, 0.0f, 0.0f };

      // compute the sine and cosine compontents
      cr::sincosf (cr::degreesToRadians (x), cr::degreesToRadians (y), cr::degreesToRadians (z), sines, cosines);

      if (forward) {
         forward->x = cosines[pitch] * cosines[yaw];
         forward->y = cosines[pitch] * sines[yaw];
         forward->z = -sines[pitch];
      }

      if (right) {
         right->x = -sines[roll] * sines[pitch] * cosines[yaw] + cosines[roll] * sines[yaw];
         right->y = -sines[roll] * sines[pitch] * sines[yaw] - cosines[roll] * cosines[yaw];
         right->z = -sines[roll] * cosines[pitch];
      }

      if (upward) {
         upward->x = cosines[roll] * sines[pitch] * cosines[yaw] + sines[roll] * sines[yaw];
         upward->y = cosines[roll] * sines[pitch] * sines[yaw] - sines[roll] * cosines[yaw];
         upward->z = cosines[roll] * cosines[pitch];
      }
   }

   const Vector &forward () {
      static Vector s_fwd {};
      buildVectors (&s_fwd, nullptr, nullptr);

      return s_fwd;
   }

   const Vector &upward () {
      static Vector s_up {};
      buildVectors (nullptr, nullptr, &s_up);

      return s_up;
   }

   const Vector &right () {
      static Vector s_right {};
      buildVectors (nullptr, &s_right, nullptr);

      return s_right;
   }
};

// expose global null vector
static auto &nullvec = Vector::null ();

CR_NAMESPACE_END