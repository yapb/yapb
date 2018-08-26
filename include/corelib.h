//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include <platform.h>

#ifdef PLATFORM_WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef ENABLE_SSE_INTRINSICS
#include <emmintrin.h>
#endif

//
// Basic Types
//
typedef signed char int8;
typedef signed short int16;
typedef signed long int32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;

// From metamod-p
static inline bool A_checkptr (const void *ptr) {
#ifdef PLATFORM_WIN32
   if (IsBadCodePtr ((FARPROC)ptr))
      return false;
#endif

   (void)(ptr);
   return true;
}

#ifndef PLATFORM_WIN32
#define _unlink unlink
#define _mkdir(p) mkdir (p, 0777)
#define stricmp strcasecmp
#else
#define stricmp _stricmp
#endif

#define A_bufsize(value) (sizeof (value) - 1)
#define A_stringify(value) (#value)

namespace Math {
static constexpr float MATH_ONEPSILON = 0.01f;
static constexpr float MATH_EQEPSILON = 0.001f;
static constexpr float MATH_FLEPSILON = 1.192092896e-07f;

static constexpr float MATH_PI = 3.141592653589793238462643383279502884f;
static constexpr float MATH_D2R = MATH_PI / 180.0f;
static constexpr float MATH_R2D = 180.0f / MATH_PI;

#ifdef ENABLE_SSE_INTRINSICS

static inline __m128 sse_abs (__m128 val) {
   return _mm_andnot_ps (_mm_castsi128_ps (_mm_set1_epi32 (0x80000000)), val);
};

static inline __m128 sse_sine (__m128 inp) {
   __m128 pi2 = _mm_set1_ps (MATH_PI * 2);
   __m128 val = _mm_cmpnlt_ps (inp, _mm_set1_ps (MATH_PI));

   val = _mm_and_ps (val, pi2);
   inp = _mm_sub_ps (inp, val);
   val = _mm_cmpngt_ps (inp, _mm_set1_ps (-MATH_PI));
   val = _mm_and_ps (val, pi2);
   inp = _mm_add_ps (inp, val);
   val = _mm_mul_ps (sse_abs (inp), _mm_set1_ps (-4.0f / (MATH_PI * MATH_PI)));
   val = _mm_add_ps (val, _mm_set1_ps (4.0f / MATH_PI));

   __m128 res = _mm_mul_ps (val, inp);

   val = _mm_mul_ps (sse_abs (res), res);
   val = _mm_sub_ps (val, res);
   val = _mm_mul_ps (val, _mm_set1_ps (0.225f));
   res = _mm_add_ps (val, res);

   return res;
}
#endif
static inline float A_sqrtf (float value) {
#ifdef ENABLE_SSE_INTRINSICS
   return _mm_cvtss_f32 (_mm_sqrt_ss (_mm_set1_ps (value)));
#else
   return sqrtf (value);
#endif
}

static inline float A_square (const float value) {
   return value * value;
}

template <typename T> static inline T A_min (T a, T b) {
   return a < b ? a : b;
}

template <typename T> static inline T A_max (T a, T b) {
   return a > b ? a : b;
}

template <typename T> static inline T A_clamp (T x, T a, T b) {
   return A_min (A_max (x, a), b);
}

static inline float F_clamp (float x, float a, float b) {
#ifdef ENABLE_SSE_INTRINSICS
   return _mm_cvtss_f32 (_mm_min_ss (_mm_max_ss (_mm_set1_ps (x), _mm_set1_ps (a)), _mm_set1_ps (b)));
#else
   return A_clamp<float> (x, a, b);
#endif
}

static inline float A_sinf (float value) {
#ifdef ENABLE_SSE_INTRINSICS
   return _mm_cvtss_f32 (sse_sine (_mm_set1_ps (value)));
#else
   return sinf (value);
#endif
}

static inline float A_cosf (float value) {
#ifdef ENABLE_SSE_INTRINSICS
   return _mm_cvtss_f32 (sse_sine (_mm_set1_ps (value + MATH_PI / 2.0f)));
#else
   return cosf (value);
#endif
}

static inline void A_sincosf (float rad, float *sine, float *cosine) {
#ifdef ENABLE_SSE_INTRINSICS
   __m128 m_sincos = sse_sine (_mm_set_ps (0.0f, 0.0f, rad + MATH_PI / 2.f, rad));
   __m128 m_cos = _mm_shuffle_ps (m_sincos, m_sincos, _MM_SHUFFLE (0, 0, 0, 1));

   *sine = _mm_cvtss_f32 (m_sincos);
   *cosine = _mm_cvtss_f32 (m_cos);
#else
   *sine = sinf (rad);
   *cosine = cosf (rad);
#endif
}

static inline bool isFltZero (float entry) {
   return fabsf (entry) < MATH_ONEPSILON;
}

static inline bool isFltEqual (float entry1, float entry2) {
   return fabsf (entry1 - entry2) < MATH_EQEPSILON;
}

static inline float radToDeg (float radian) {
   return radian * MATH_R2D;
}

static inline float degToRad (float degree) {
   return degree * MATH_D2R;
}

static inline float angleMod (float angle) {
   return 360.0f / 65536.0f * (static_cast<int> (angle * (65536.0f / 360.0f)) & 65535);
}

static inline float angleNorm (float angle) {
   return 360.0f / 65536.0f * (static_cast<int> ((angle + 180.0f) * (65536.0f / 360.0f)) & 65535) - 180.0f;
}

static inline void sincosf (float rad, float *sine, float *cosine) {
#if defined(__ANDROID__)
   *sine = sinf (rad);
   *cosine = cosf (rad);
#else
   A_sincosf (rad, sine, cosine);
#endif
}

static inline float angleDiff (float dest, float src) {
   return angleNorm (dest - src);
}
}

// see: https://github.com/preshing/RandomSequence/
class RandomSequenceOfUnique {
private:
   unsigned int m_index;
   unsigned int m_intermediateOffset;
   unsigned long long m_divider;

private:
   unsigned int premute (unsigned int x) {
      static const unsigned int prime = 4294967291u;

      if (x >= prime) {
         return x;
      }
      unsigned int residue = (static_cast<unsigned long long> (x) * x) % prime;
      return (x <= prime / 2) ? residue : prime - residue;
   }

   unsigned int random (void) {
      return premute ((premute (m_index++) + m_intermediateOffset) ^ 0x5bf03635);
   }

public:
   RandomSequenceOfUnique (void) {
      unsigned int seedBase = static_cast<unsigned int> (time (nullptr));
      unsigned int seedOffset = seedBase + 1;

      m_index = premute (premute (seedBase) + 0x682f0161);
      m_intermediateOffset = premute (premute (seedOffset) + 0x46790905);
      m_divider = (static_cast<unsigned long long> (1)) << 32;
   }

   inline int getInt (int low, int high) {
      return static_cast<int> (random () * (static_cast<double> (high) - static_cast<double> (low) + 1.0) / m_divider + static_cast<double> (low));
   }

   inline float getFloat (float low, float high) {
      return static_cast<float> (random () * (static_cast<double> (high) - static_cast<double> (low)) / (m_divider - 1) + static_cast<double> (low));
   }
};

class Vector {
public:
   float x, y, z;

public:
   inline Vector (float scaler = 0.0f) : x (scaler), y (scaler), z (scaler) {}
   inline Vector (float inputX, float inputY, float inputZ) : x (inputX) , y (inputY) , z (inputZ) {}
   inline Vector (float *other) : x (other[0]) , y (other[1]) , z (other[2]) {}
   inline Vector (const Vector &right) : x (right.x) , y (right.y) , z (right.z) {}

public:
   inline operator float * (void) {
      return &x;
   }

   inline operator const float * (void)const {
      return &x;
   }

   inline const Vector operator+ (const Vector &right) const {
      return Vector (x + right.x, y + right.y, z + right.z);
   }

   inline const Vector operator- (const Vector &right) const {
      return Vector (x - right.x, y - right.y, z - right.z);
   }

   inline const Vector operator- (void) const {
      return Vector (-x, -y, -z);
   }

   friend inline const Vector operator* (const float vec, const Vector &right) {
      return Vector (right.x * vec, right.y * vec, right.z * vec);
   }

   inline const Vector operator* (float vec) const {
      return Vector (vec * x, vec * y, vec * z);
   }

   inline const Vector operator/ (float vec) const {
      const float inv = 1 / vec;
      return Vector (inv * x, inv * y, inv * z);
   }

   // cross product
   inline const Vector operator^ (const Vector &right) const {
      return Vector (y * right.z - z * right.y, z * right.x - x * right.z, x * right.y - y * right.x);
   }

   // dot product
   inline float operator| (const Vector &right) const {
      return x * right.x + y * right.y + z * right.z;
   }

   inline const Vector &operator+= (const Vector &right) {
      x += right.x;
      y += right.y;
      z += right.z;

      return *this;
   }

   inline const Vector &operator-= (const Vector &right) {
      x -= right.x;
      y -= right.y;
      z -= right.z;

      return *this;
   }

   inline const Vector &operator*= (float vec) {
      x *= vec;
      y *= vec;
      z *= vec;

      return *this;
   }

   inline const Vector &operator/= (float vec) {
      const float inv = 1 / vec;

      x *= inv;
      y *= inv;
      z *= inv;

      return *this;
   }

   inline bool operator== (const Vector &right) const {
      return Math::isFltEqual (x, right.x) && Math::isFltEqual (y, right.y) && Math::isFltEqual (z, right.z);
   }

   inline bool operator!= (const Vector &right) const {
      return !Math::isFltEqual (x, right.x) && !Math::isFltEqual (y, right.y) && !Math::isFltEqual (z, right.z);
   }

   inline const Vector &operator= (const Vector &right) {
      x = right.x;
      y = right.y;
      z = right.z;

      return *this;
   }

public:
   inline float length (void) const {
      return Math::A_sqrtf (x * x + y * y + z * z);
   }

   inline float length2D (void) const {
      return Math::A_sqrtf (x * x + y * y);
   }

   inline float lengthSq (void) const {
      return x * x + y * y + z * z;
   }

   inline Vector make2D (void) const {
      return Vector (x, y, 0.0f);
   }

   inline Vector normalize (void) const {
      float len = length () + static_cast<float> (Math::MATH_FLEPSILON);

      if (Math::isFltZero (len)) {
         return Vector (0.0f, 0.0f, 1.0f);
      }
      len = 1.0f / len;
      return Vector (x * len, y * len, z * len);
   }

   inline Vector normalize2D (void) const {
      float len = length2D () + static_cast<float> (Math::MATH_FLEPSILON);

      if (Math::isFltZero (len)) {
         return Vector (0.0f, 1.0f, 0.0f);
      }
      len = 1.0f / len;
      return Vector (x * len, y * len, 0.0f);
   }

   inline bool empty (void) const {
      return Math::isFltZero (x) && Math::isFltZero (y) && Math::isFltZero (z);
   }

   inline static const Vector &null (void) {
      static const Vector s_zero = Vector (0.0f, 0.0f, 0.0f);
      return s_zero;
   }

   inline void nullify (void) {
      x = y = z = 0.0f;
   }

   inline Vector clampAngles (void) {
      x = Math::angleNorm (x);
      y = Math::angleNorm (y);
      z = 0.0f;

      return *this;
   }

   inline float toPitch (void) const {
      if (Math::isFltZero (x) && Math::isFltZero (y)) {
         return 0.0f;
      }
      return Math::radToDeg (atan2f (z, length2D ()));
   }

   inline float toYaw (void) const {
      if (Math::isFltZero (x) && Math::isFltZero (y)) {
         return 0.0f;
      }
      return Math::radToDeg (atan2f (y, x));
   }

   inline Vector toAngles (void) const {
      if (Math::isFltZero (x) && Math::isFltZero (y)) {
         return Vector (z > 0.0f ? 90.0f : 270.0f, 0.0, 0.0f);
      }
      return Vector (Math::radToDeg (atan2f (z, length2D ())), Math::radToDeg (atan2f (y, x)), 0.0f);
   }

   inline void makeVectors (Vector *forward, Vector *right, Vector *upward) const {
      float sinePitch = 0.0f, cosinePitch = 0.0f, sineYaw = 0.0f, cosineYaw = 0.0f, sineRoll = 0.0f, cosineRoll = 0.0f;

      Math::sincosf (Math::degToRad (x), &sinePitch, &cosinePitch); // compute the sine and cosine of the pitch component
      Math::sincosf (Math::degToRad (y), &sineYaw, &cosineYaw); // compute the sine and cosine of the yaw component
      Math::sincosf (Math::degToRad (z), &sineRoll, &cosineRoll); // compute the sine and cosine of the roll component

      if (forward) {
         forward->x = cosinePitch * cosineYaw;
         forward->y = cosinePitch * sineYaw;
         forward->z = -sinePitch;
      }

      if (right) {
         right->x = -sineRoll * sinePitch * cosineYaw + cosineRoll * sineYaw;
         right->y = -sineRoll * sinePitch * sineYaw - cosineRoll * cosineYaw;
         right->z = -sineRoll * cosinePitch;
      }

      if (upward) {
         upward->x = cosineRoll * sinePitch * cosineYaw + sineRoll * sineYaw;
         upward->y = cosineRoll * sinePitch * sineYaw - sineRoll * cosineYaw;
         upward->z = cosineRoll * cosinePitch;
      }
   }
};

class Library {
private:
   void *m_ptr;

public:
   Library (const char *filename)
      : m_ptr (nullptr) {

      if (!filename) {
         return;
      }
      load (filename);
   }

   virtual ~Library (void) {
      if (!isValid ()) {
         return;
      }
#ifdef PLATFORM_WIN32
      FreeLibrary ((HMODULE)m_ptr);
#else
      dlclose (m_ptr);
#endif
   }

public:
   inline void *load (const char *filename) {
#ifdef PLATFORM_WIN32
      m_ptr = LoadLibrary (filename);
#else
      m_ptr = dlopen (filename, RTLD_NOW);
#endif
      return m_ptr;
   }

   template <typename R> R resolve (const char *function) {
      if (!isValid ()) {
         return nullptr;
      }
      return (R)
#ifdef PLATFORM_WIN32
         GetProcAddress (static_cast<HMODULE> (m_ptr), function);
#else
         dlsym (m_ptr, function);
#endif
   }

   template <typename R> R handle (void) {
      return (R)m_ptr;
   }

   inline bool isValid (void) const {
      return m_ptr != nullptr;
   }
};

template <typename A, typename B> class Pair {
public:
   A first;
   B second;

public:
   Pair (A first, B second) {
      this->first = first;
      this->second = second;
   }

   Pair (void) = default;
   ~Pair (void) = default;

public:
   static Pair<A, B> make (A first, B second) {
      return Pair<A, B> (first, second);
   }
};

template <typename T> class Array {
private:
   T *m_data;
   size_t m_capacity, m_length;

public:
   Array (void)
      : m_data (nullptr)
      , m_capacity (0)
      , m_length (0) {}

   Array (const Array &other)
      : m_data (nullptr)
      , m_capacity (0)
      , m_length (0) {
      assign (other);
   }

   virtual ~Array (void) {
      destroy ();
   }

public:
   void destroy (void) {
      delete[] m_data;

      m_data = nullptr;
      m_capacity = 0;
      m_length = 0;
   }

   bool reserve (size_t newSize) {
      if (!newSize) {
         destroy ();
         return true;
      }

      if (m_length + newSize < m_capacity) {
         return true;
      }
      size_t maxSize = m_capacity ? m_capacity : 8;

      while (m_length + newSize > maxSize) {
         maxSize *= 2;
      }
      T *buffer = new T[maxSize];

      if (m_data != nullptr) {
         if (maxSize < m_length) {
            m_length = maxSize;
         }
         for (size_t i = 0; i < m_length; i++) {
            buffer[i] = m_data[i];
         }
      }
      delete[] m_data;

      m_data = buffer;
      m_capacity = maxSize;

      return true;
   }

   bool resize (size_t newSize) {
      m_length = newSize;
      return reserve (newSize);
   }

   size_t length (void) const {
      return m_length;
   }

   size_t capacity (void) const {
      return m_capacity;
   }

   bool set (size_t index, T object) {
      if (index >= m_capacity) {
         if (!reserve (index + 2)) {
            return false;
         }
      }
      m_data[index] = object;

      if (index >= m_length) {
         m_length = index + 1;
      }
      return true;
   }

   T &at (size_t index) {
      return m_data[index];
   }

   bool at (size_t index, T &object) {
      if (index >= m_length) {
         return false;
      }
      object = m_data[index];
      return true;
   }

   bool insert (size_t index, T object) {
      return insert (index, &object, 1);
   }

   bool insert (size_t index, T *objects, size_t count = 1) {
      if (!objects || count < 1) {
         return false;
      }

      size_t newSize = m_length > index ? m_length : index;
      newSize += count;

      if (newSize >= m_capacity && !reserve (newSize + 3)) {
         return false;
      }

      if (index >= m_length) {
         for (size_t i = 0; i < count; i++) {
            m_data[i + index] = objects[i];
         }
         m_length = newSize;
      }
      else {
         size_t i = 0;

         for (i = m_length; i > index; i--) {
            m_data[i + count - 1] = m_data[i - 1];
         }
         for (i = 0; i < count; i++) {
            m_data[i + index] = objects[i];
         }
         m_length += count;
      }
      return true;
   }

   bool insert (size_t index, Array &other) {
      if (&other == this) {
         return false;
      }
      return insert (index, other.m_data, other.m_length);
   }

   bool erase (size_t index, size_t count = 1) {
      if (index + count > m_capacity) {
         return false;
      }
      m_length -= count;

      for (size_t i = index; i < m_length; i++) {
         m_data[i] = m_data[i + count];
      }
      return true;
   }

   bool erase (T &item) {
      return erase (index (item), 1);
   }

   bool push (T object) {
      return insert (m_length, &object);
   }

   bool push (T *objects, size_t count = 1) {
      return insert (m_length, objects, count);
   }

   bool push (Array &other) {
      if (&other == this) {
         return false;
      }
      return insert (m_length, other.m_data, other.m_length);
   }

   void clear (void) {
      m_length = 0;
   }

   inline bool empty (void) const {
      return m_length <= 0;
   }

   void shrink (bool destroyEmpty = true) {
      if (!m_length) {
         if (destroyEmpty) {
            destroy ();
         }
         return;
      }
      T *buffer = new T[m_length];

      if (m_data != nullptr) {
         for (size_t i = 0; i < m_length; i++) {
            buffer[i] = m_data[i];
         }
      }
      delete[] m_data;

      m_data = buffer;
      m_capacity = m_length;
   }

   size_t index (T &item) {
      return &item - &m_data[0];
   }

   T &pop (void) {
      T &element = m_data[m_length - 1];
      erase (m_length - 1);

      return element;
   }

   const T &back (void) const {
      return m_data[m_length - 1];
   }

   T &back (void) {
      return m_data[m_length - 1];
   }

   bool last (T &item) {
      if (m_length <= 0) {
         return false;
      }
      item = m_data[m_length - 1];
      return true;
   }

   bool assign (const Array &other) {
      if (&other == this) {
         return true;
      }
      if (!reserve (other.m_length)) {
         return false;
      }
      for (size_t i = 0; i < other.m_length; i++) {
         m_data[i] = other.m_data[i];
      }
      m_length = other.m_length;
      return true;
   }

   T &random (void) const {
      extern RandomSequenceOfUnique rng;
      return m_data[rng.getInt (0, m_length - 1)];
   }

   Array &operator= (const Array &other) {
      assign (other);
      return *this;
   }

   T &operator[] (size_t index) {
      if (index < m_capacity && index >= m_length) {
         m_length = index + 1;
      }
      return at (index);
   }

   T *begin (void) {
      return m_data;
   }

   T *begin (void) const {
      return m_data;
   }

   T *end (void) {

      return m_data + m_length;
   }

   T *end (void) const {
      return m_data + m_length;
   }
};

class String {
public:
   static constexpr size_t NPOS = static_cast<size_t> (-1);

private:
   char *m_chars;
   size_t m_capacity, m_length;

protected:
   void grow (size_t size) {
      if (size <= m_capacity) {
         return;
      }

      m_capacity = size + sizeof (char) * 2;
      char *chars = new char[size + 1];

      if (m_chars) {
         strcpy (chars, m_chars);
         chars[m_length] = 0;

         delete[] m_chars;
      }
      m_chars = chars;
      m_capacity = size;
   }

   void moveItems (size_t dst, size_t src) {
      memmove (m_chars + dst, m_chars + src, sizeof (char) * (m_length - src + 1));
   }

   bool checkTrimChar (char ch, const char *chars) {
      do {
         if (*chars == ch) {
            return true;
         }
         chars++;
      } while (*chars);
      return false;
   }

   void allocateSpace (int length) {
      int maxSize = static_cast<int> (m_capacity - m_length) - 1;

      if (length <= maxSize) {
         return;
      }
      int delta = 8;

      if (m_capacity > 64) {
         delta = m_capacity / 2;
      }
      else if (m_capacity > 8) {
         delta = 16;
      }

      if (maxSize + delta < length) {
         delta = length - maxSize;
      }

      if (delta < 0) {
         delta = 0;
      }
      grow (m_capacity + static_cast<size_t> (delta));
   }

   void putSpace (size_t &index, size_t size) {
      if (index > m_length) {
         index = m_length;
      }
      allocateSpace (size);
      moveItems (index + size, index);
   }

public:
   String (void)
      : m_chars (nullptr)
      , m_capacity (0)
      , m_length (0) {}

   virtual ~String (void) {
      delete[] m_chars;
   }

   String (const char *input)
      : m_chars (nullptr)
      , m_capacity (0)
      , m_length (0) {
      assign (input);
   }

   String (char input)
      : m_chars (nullptr)
      , m_capacity (0)
      , m_length (0) {
      assign (input);
   }

   String (const String &input)
      : m_chars (nullptr)
      , m_capacity (0)
      , m_length (0) {
      assign (input.chars ());
   }

public:
   char *begin (void) {
      return m_chars;
   }

   char *begin (void) const {
      return m_chars;
   }

   char *end (void) {
      return m_chars + m_length;
   }

   char *end (void) const {
      return (m_chars + m_length);
   }

   const char *chars (void) const {
      if (!m_chars)
         return "";

      return m_chars;
   }

   String &append (const char *buffer) {
      grow (m_length + strlen (buffer) + 1);
      strcat (m_chars, buffer);

      m_length = strlen (m_chars);
      return *this;
   }

   String &append (const char input) {
      grow (m_length + 2);

      m_chars[m_length] = input;
      m_chars[++m_length] = 0;

      return *this;
   }

   String &append (const String &input) {
      const char *buffer = input.chars ();
      grow (m_length + strlen (buffer));

      strcat (m_chars, buffer);
      m_length = strlen (m_chars);

      return *this;
   }

   void assign (const String &input) {
      assign (input.chars ());
   }

   void assign (char input) {
      char psz[2] = {input, 0};
      assign (psz);
   }

   void assign (const char *input) {
      if (!input) {
         grow (1);
         m_length = 0;
         return;
      }
      grow (strlen (input));

      if (m_chars) {
         strcpy (m_chars, input);
         m_length = strlen (m_chars);
      }
      else
         m_length = 0;
   }

   template <int BufferSize = 512> void format (const char *fmt, ...) {
      va_list ap;
      char buffer[BufferSize];

      va_start (ap, fmt);
      vsnprintf (buffer, sizeof (buffer) - 1, fmt, ap);
      va_end (ap);

      assign (buffer);
   }

   template <int BufferSize = 512> void formatAppend (const char *fmt, ...) {
      va_list ap;
      char buffer[BufferSize];

      va_start (ap, fmt);
      vsnprintf (buffer, sizeof (buffer) - 1, fmt, ap);
      va_end (ap);

      append (buffer);
   }

   void clear (void) {
      if (m_chars != nullptr) {
         m_chars[0] = 0;
         m_length = 0;
      }
   }

   bool empty (void) const {
      return !length ();
   }

   size_t length (void) const {
      return m_chars ? m_length : 0;
   }

   int toInt32 (void) const {
      return atoi (chars ());
   }

   friend String operator+ (const String &lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator+ (const String &lhs, char rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator+ (char lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator+ (const String &lhs, const char *rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator+ (const char *lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend bool operator== (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) == 0;
   }

   friend bool operator< (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) < 0;
   }

   friend bool operator> (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) > 0;
   }

   friend bool operator== (const char *lhs, const String &rhs) {
      return rhs.compare (lhs) == 0;
   }

   friend bool operator== (const String &lhs, const char *rhs) {
      return lhs.compare (rhs) == 0;
   }

   friend bool operator!= (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) != 0;
   }

   friend bool operator!= (const char *lhs, const String &rhs) {
      return rhs.compare (lhs) != 0;
   }

   friend bool operator!= (const String &lhs, const char *rhs) {
      return lhs.compare (rhs) != 0;
   }

   String &operator= (const String &rhs) {
      assign (rhs);
      return *this;
   }

   String &operator= (const char *rhs) {
      assign (rhs);
      return *this;
   }

   String &operator= (char rhs) {
      assign (rhs);
      return *this;
   }

   String &operator+= (const String &rhs) {
      append (rhs);
      return *this;
   }

   String &operator+= (const char *rhs) {
      append (rhs);
      return *this;
   }

   char operator[] (size_t index) const {
      return m_chars[index];
   }

   char &operator[] (size_t index) {
      return m_chars[index];
   }

   int compare (const String &what) const {
      return strcmp (m_chars, what.m_chars);
   }

   int compare (const char *what) const {
      return strcmp (m_chars, what);
   }

   bool contains (const String &what) const {
      return strstr (m_chars, what.m_chars) != nullptr;
   }

   int erase (size_t index, size_t count = 1) {
      if (index + count > m_length) {
         count = m_length - index;
      }

      if (count > 0) {
         moveItems (index, index + count);
         m_length -= count;
      }
      return m_length;
   }

   size_t replace (const String &what, const String &to) {
      if (!what.length () || !to.length ()) {
         return 0;
      }

      size_t num = 0;
      size_t pos = 0;

      while (pos < m_length) {
         pos = find (what, pos);

         if (pos == NPOS) {
            break;
         }
         erase (pos, what.length ());
         insert (pos, to);

         pos += to.length ();
         num++;
      }
      return num;
   }

   size_t insert (size_t pos, const String &input) {
      if (!input.length ()) {
         return m_length;
      }

      if (pos > m_length) {
         pos = m_length;
      }

      int numInsertChars = input.length ();
      putSpace (pos, numInsertChars);

      for (int i = 0; i < numInsertChars; i++) {
         m_chars[pos + i] = input[i];
      }
      m_length += numInsertChars;

      return m_length;
   }

   size_t find (char input, size_t start) const {
      char *str = m_chars + start;

      while (true) {
         if (*str == input) {
            return str - m_chars;
         }

         if (!*str) {
            return NPOS;
         }
         str++;
      }
      return NPOS;
   }

   size_t find (const String &search, size_t pos) const {
      if (pos > m_length) {
         return NPOS;
      }
      size_t inputLength = search.length ();

      for (size_t i = pos; inputLength + i <= m_length; i++) {
         if (strncmp (m_chars + i, search.chars (), inputLength) == 0) {
            return i;
         }
      }
      return NPOS;
   }

   String substr (size_t start, size_t count = NPOS) {
      String out;

      if (start >= m_length || !m_chars) {
         return out;
      }

      if (count == NPOS) {
         count = m_length - start;
      }
      else if (start + count >= m_length) {
         count = m_length - start;
      }
      size_t j = 0;
      char *holder = new char[m_length + 1];

      for (size_t i = start; i < start + count; i++)
         holder[j++] = m_chars[i];

      holder[j] = '\0';
      out.assign (holder);

      delete[] holder;
      return out;
   }

   Array <String> split (const char *sep) {
      Array <String> tokens;
      size_t length, index = 0;

      do {
         index += strspn (&m_chars[index], sep);
         length = strcspn (&m_chars[index], sep);

         if (length > 0) {
            tokens.push (substr (index, length));
         }
         index += length;
      } while (length > 0);

      return tokens;
   }

   String &trimRight (const char *chars = "\r\n\t ") {
      char *str = m_chars + m_length - 1;

      while (*str != 0) {
         if (checkTrimChar (*str, chars)) {
            erase (str - m_chars);
         }
         else {
            break;
         }
         str--;
      }
      return *this;
   }

   String &trimLeft (const char *chars = "\r\n\t ") {
      char *str = m_chars;

      while (checkTrimChar (*str, chars))
         str++;

      if (m_chars != str) {
         erase (0, str - m_chars);
      }
      return *this;
   }

   String &trim (const char *chars = "\r\n\t ") {
      return trimLeft (chars).trimRight (chars);
   }

public:
   static void trimChars (char *str) {
      size_t pos = 0;
      char *dest = str;

      while (str[pos] <= ' ' && str[pos] > 0) {
         pos++;
      }

      while (str[pos]) {
         *(dest++) = str[pos];
         pos++;
      }
      *(dest--) = '\0';

      while (dest >= str && *dest <= ' ' && *dest > 0) {
         *(dest--) = '\0';
      }
   }
};

template <typename K> struct MapKeyHash {
   constexpr unsigned long operator () (const K &key) const
   {
      size_t hash = 5321;
      char *str = const_cast <char *> (key.chars ());
      char ch;

      while (ch = *str++) {
         hash = ((hash << 5) + hash) + ch;
      }
      return hash;
   }
};


template <typename K, typename V, typename H = MapKeyHash <K>, size_t I = 256> class HashMap {
public:
   using Bucket = Pair <K, V>;

private:
   Array<Bucket> m_buckets[I];
   H m_hash;

   auto &getBucket (const K &key) {
      return m_buckets[m_hash (key) % I];
   }

public:

   bool exists (const K &key) {
      return get (key, nullptr);
   }

   bool get (const K &key, V *val) {
      for (auto &entry : getBucket (key)) {
         if (entry.first == key) {
            if (val != nullptr) {
               *val = entry.second;
            }
            return true;
         }
      }
      return false;
   }

   bool get (const K &key, V &val) {
      return get (key, &val);
   }

   bool put (const K &key, const V &val) {
      if (exists (key)) {
         return false;
      }
      getBucket (key).push (Pair <K, V>::make (key, val));
      return true;
   }

   bool erase (const K &key) {
      auto &bucket = getBucket (key);

      for (auto &entry : getBucket (key)) {
         if (entry.first == key) {
            bucket.erase (entry);
            return true;
         }
      }
      return false;
   }

public:
   V & operator [] (const K &key) {
      V res;
      get (key, res);

      return res;
   }
};

class File {
private:
   FILE *m_handle;
   size_t m_size;

public:
   File (void)
      : m_handle (nullptr)
      , m_size (0) {}

   File (const String &fileName, const String &mode = "rt") : m_size (0){
      open (fileName, mode);
   }

   virtual ~File (void) {
      close ();
   }

   bool open (const String &fileName, const String &mode) {
      if ((m_handle = fopen (fileName.chars (), mode.chars ())) == nullptr) {
         return false;
      }
      fseek (m_handle, 0L, SEEK_END);
      m_size = ftell (m_handle);
      fseek (m_handle, 0L, SEEK_SET);

      return true;
   }

   void close (void) {
      if (m_handle != nullptr) {
         fclose (m_handle);
         m_handle = nullptr;
      }
      m_size = 0;
   }

   bool eof (void) {
      return feof (m_handle) ? true : false;
   }

   bool flush (void) {
      return fflush (m_handle) ? false : true;
   }

   int getch (void) {
      return fgetc (m_handle);
   }

   char *gets (char *buffer, int count) {
      return fgets (buffer, count, m_handle);
   }

   int writeFormat (const char *format, ...) {
      assert (m_handle != nullptr);

      va_list ap;
      va_start (ap, format);
      int written = vfprintf (m_handle, format, ap);
      va_end (ap);

      if (written < 0) {
         return 0;
      }
      return written;
   }

   char putch (char ch) {
      return static_cast<char> (fputc (ch, m_handle));
   }

   bool puts (const String &buffer) {
      assert (m_handle != nullptr);

      if (fputs (buffer.chars (), m_handle) < 0) {
         return false;
      }
      return true;
   }

   int read (void *buffer, int size, int count = 1) {
      return fread (buffer, size, count, m_handle);
   }

   int write (void *buffer, int size, int count = 1) {
      return fwrite (buffer, size, count, m_handle);
   }

   bool seek (long offset, int origin) {
      if (fseek (m_handle, offset, origin) != 0) {
         return false;
      }
      return true;
   }

   void rewind (void) {
      ::rewind (m_handle);
   }

   size_t getSize (void) const {
      return m_size;
   }

   bool isValid (void) const {
      return m_handle != nullptr;
   }

public:
   static inline bool exists (const String &filename) {
      File fp;

      if (fp.open (filename, "rb")) {
         fp.close ();
         return true;
      }
      return false;
   }

   static inline void pathCreate (char *path) {
      for (char *str = path + 1; *str; str++) {
         if (*str == '/') {
            *str = 0;
            _mkdir (path);
            *str = '/';
         }
      }
      _mkdir (path);
   }
};

class MemFile {
public:
   typedef uint8 *(*MF_Loader) (const char *, int *);
   typedef void (*MF_Unloader) (uint8 *);

public:
   static MF_Loader Loader;
   static MF_Unloader Unloader;

private:
   size_t m_size;
   size_t m_pos;
   uint8 *m_buffer;

public:
   MemFile (void)
      : m_size (0)
      , m_pos (0)
      , m_buffer (nullptr) {}

   MemFile (const String &filename)
      : m_size (0)
      , m_pos (0)
      , m_buffer (nullptr) {
      open (filename);
   }

   virtual ~MemFile (void) {
      close ();
   }

   bool open (const String &filename) {
      if (!Loader) {
         return false;
      }
      m_size = 0;
      m_pos = 0;
      m_buffer = Loader (filename.chars (), reinterpret_cast<int *> (&m_size));

      if (!m_buffer) {
         return false;
      }
      return true;
   }

   void close (void) {
      if (Unloader) {
         Unloader (m_buffer);
      }
      m_size = 0;
      m_pos = 0;
      m_buffer = nullptr;
   }

   int getch (void) {
      if (!m_buffer || m_pos >= m_size) {
         return -1;
      }
      int readCh = m_buffer[m_pos];
      m_pos++;

      return readCh;
   }

   char *gets (char *buffer, size_t count) {
      if (!m_buffer || m_pos >= m_size) {
         return nullptr;
      }
      size_t index = 0;
      buffer[0] = 0;

      for (; index < count - 1;) {
         if (m_pos < m_size) {
            buffer[index] = m_buffer[m_pos++];

            if (buffer[index++] == '\n') {
               break;
            }
         }
         else {
            break;
         }
      }
      buffer[index] = 0;
      return index ? buffer : nullptr;
   }

   int read (void *buffer, size_t size, size_t count = 1) {
      if (!m_buffer || m_size <= m_pos || !buffer || !size || !count) {
         return 0;
      }
      size_t blocksRead = size * count <= m_size - m_pos ? size * count : m_size - m_pos;

      memcpy (buffer, &m_buffer[m_pos], blocksRead);
      m_pos += blocksRead;

      return blocksRead / size;
   }

   bool seek (size_t offset, int origin) {
      if (!m_buffer || m_pos >= m_size) {
         return false;
      }
      if (origin == SEEK_SET) {
         if (offset >= m_size) {
            return false;
         }
         m_pos = offset;
      }
      else if (origin == SEEK_END) {
         if (offset >= m_size) {
            return false;
         }
         m_pos = m_size - offset;
      }
      else {
         if (m_pos + offset >= m_size) {
            return false;
         }
         m_pos += offset;
      }
      return true;
   }

   inline int getSize (void) const {
      return m_size;
   }

   bool isValid (void) const {
      return m_buffer && m_size > 0;
   }
};

using StringArray = Array <String>;
using IntArray = Array <int>;

#ifndef FORCEINLINE
#define FORCEINLINE inline
#endif

template <typename T> class Singleton {
protected:
   Singleton (void) {}
   virtual ~Singleton (void) {}

private:
   Singleton (const Singleton &) = delete;
   Singleton &operator= (const Singleton &) = delete;

public:
   static FORCEINLINE T *ptr (void) {
      return &ref ();
   }

   static FORCEINLINE T &ref (void) {
      static T ref;
      return ref;
   };
};
