//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//
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
#include <time.h>
#include <math.h>

#include <platform.h>

#ifdef PLATFORM_WIN32
   #include <direct.h>
#else
   #include <sys/stat.h>
#endif

#ifndef PLATFORM_WIN32
   #define _unlink unlink
   #define _mkdir(p) mkdir (p, 0777)
   #define stricmp strcasecmp
#else
   #define stricmp _stricmp
#endif

#undef min
#undef max

// to remove ugly A_ prefixes
namespace cr {

namespace types {

using int8 = signed char;
using int16 = signed short;
using int32 = signed long;
using uint8 = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned long;

}

using namespace cr::types;

constexpr float ONEPSILON = 0.01f;
constexpr float EQEPSILON = 0.001f;
constexpr float FLEPSILON = 1.192092896e-07f;

constexpr float PI = 3.141592653589793115997963468544185161590576171875f;
constexpr float PI_RECIPROCAL = 1.0f / PI;
constexpr float PI_HALF = PI / 2;

constexpr float D2R = PI / 180.0f;
constexpr float R2D = 180.0f / PI;

namespace cmath {
using ::atan2f;
using ::ceilf;
using ::log10f;
}

// from metamod-p
static inline bool checkptr (const void *ptr) {
#ifdef PLATFORM_WIN32
   if (IsBadCodePtr (reinterpret_cast <FARPROC> (ptr)))
      return false;
#endif

   (void)(ptr);
   return true;
}

template <typename T, size_t N> constexpr size_t bufsize (const T (&)[N]) {
   return N - 1;
}

template <typename T, size_t N> constexpr size_t arrsize (const T (&)[N]) {
   return N;
}

constexpr float square (const float value) {
   return value * value;
}

template <typename T> constexpr T min (const T a, const T b) {
   return a < b ? a : b;
}

template <typename T> constexpr T max (const T a, const T b) {
   return a > b ? a : b;
}

template <typename T> constexpr T clamp (const T x, const T a, const T b) {
   return min (max (x, a), b);
}

template <typename T> constexpr T abs (const T a) {
   return a > 0 ? a : -a;
}

static inline float powf (const float x, const float y) {
   union {
      float d;
      int x;
   } res { x };

   res.x = static_cast <int> (y * (res.x - 1064866805) + 1064866805);
   return res.d;
}

static inline float sqrtf (const float value) {
   return powf (value, 0.5f);
}

static inline float sinf (const float value) {
   const signed long sign = static_cast <signed long> (value * PI_RECIPROCAL);
   const float calc = (value - static_cast <float> (sign) * PI);

   const float sqr = square (calc);
   const float res = 1.00000000000000000000e+00f + sqr * (-1.66666671633720397949e-01f + sqr * (8.33333376795053482056e-03f + sqr * (-1.98412497411482036114e-04f +
      sqr * (2.75565571428160183132e-06f + sqr * (-2.50368472620721149724e-08f + sqr * (1.58849267073435385100e-10f + sqr * -6.58925550841432672300e-13f))))));

   return (sign & 1) ? -calc * res : value * res;
}

static inline float cosf (const float value) {
   const signed long sign = static_cast <signed long> (value * PI_RECIPROCAL);
   const float calc = (value - static_cast <float> (sign) * PI);

   const float sqr = square (calc);
   const float res = sqr * (-5.00000000000000000000e-01f + sqr * (4.16666641831398010254e-02f + sqr * (-1.38888671062886714935e-03f + sqr * (2.48006890615215525031e-05f +
      sqr * (-2.75369927749125054106e-07f + sqr * (2.06207229069832465029e-09f + sqr * -9.77507137733812925262e-12f))))));

   const float f = -1.00000000000000000000e+00f;

   return (sign & 1) ? f - res : -f + res;
}

static inline float tanf (const float value) {
   return sinf (value) / cosf (value);
}

static inline float atan2f (const float y, const float x) {
   return cmath::atan2f (y, x);
}

static inline float log10f (const float x) {
   return cmath::log10f (x);
}

static inline float ceilf (const float x) {
   return cmath::ceilf (x);
}

static inline void sincosf (const float rad, float &sine, float &cosine) {
   sine = sinf (rad);
   cosine = cosf (rad);
}

constexpr bool fzero (const float entry) {
   return abs (entry) < ONEPSILON;
}

constexpr bool fequal (const float entry1, const float entry2) {
   return abs (entry1 - entry2) < EQEPSILON;
}

constexpr float rad2deg (const float radian) {
   return radian * R2D;
}

constexpr float deg2rad (const float degree) {
   return degree * D2R;
}

constexpr float angleMod (const float angle) {
   return 360.0f / 65536.0f * (static_cast <int> (angle * (65536.0f / 360.0f)) & 65535);
}

constexpr float angleNorm (const float angle) {
   return 360.0f / 65536.0f * (static_cast <int> ((angle + 180.0f) * (65536.0f / 360.0f)) & 65535) - 180.0f;
}

constexpr float angleDiff (const float dest, const float src) {
   return angleNorm (dest - src);
}

template <typename T> struct ClearRef {
   using Type = T;
};

template <typename T> struct ClearRef <T &> {
   using Type = T;
};

template <typename T> struct ClearRef <T &&> {
   using Type = T;
};

template <typename T> static inline typename ClearRef <T>::Type &&move (T &&type) {
   return static_cast <typename ClearRef <T>::Type &&> (type);
}

template <typename T> static inline void swap (T &left, T &right) {
   auto temp = move (left);
   left = move (right);
   right = move (temp);
}

template <typename T> static constexpr inline T &&forward (typename ClearRef <T>::Type &type) noexcept {
   return static_cast <T &&> (type);
}

template <typename T> static constexpr inline T &&forward (typename ClearRef <T>::Type &&type) noexcept {
   return static_cast <T &&> (type);
}

template <typename T> static inline void moveArray (T *dest, T *src, size_t length) {
   for (size_t i = 0; i < length; i++) {
      dest[i] = move (src[i]);
   }
}

namespace classes {

template <typename T> class Singleton {
protected:
   Singleton (void) = default;
   virtual ~Singleton (void) = default;

private:
   Singleton (const Singleton &) = delete;
   Singleton &operator= (const Singleton &) = delete;

public:
   inline static T *ptr (void) {
      return &ref ();
   }

   inline static T &ref (void) {
      static T ref;
      return ref;
   };
};

// see: https://github.com/preshing/RandomSequence/
class RandomSequence : public Singleton <RandomSequence> {
private:
   unsigned int m_index;
   unsigned int m_intermediateOffset;
   unsigned long long m_divider;

private:
   unsigned int premute (unsigned int x) {
      static constexpr unsigned int prime = 4294967291u;

      if (x >= prime) {
         return x;
      }
      const unsigned int residue = (static_cast <unsigned long long> (x) * x) % prime;
      return (x <= prime / 2) ? residue : prime - residue;
   }

   unsigned int random (void) {
      return premute ((premute (m_index++) + m_intermediateOffset) ^ 0x5bf03635);
   }

public:
   RandomSequence (void) {
      const unsigned int seedBase = static_cast <unsigned int> (time (nullptr));
      const unsigned int seedOffset = seedBase + 1;

      m_index = premute (premute (seedBase) + 0x682f0161);
      m_intermediateOffset = premute (premute (seedOffset) + 0x46790905);
      m_divider = (static_cast <unsigned long long> (1)) << 32;
   }

   template <typename U> inline U getInt (U low, U high) {
      return static_cast <U> (random () * (static_cast <double> (high) - static_cast <double> (low) + 1.0) / m_divider + static_cast <double> (low));
   }

   inline float getFloat (float low, float high) {
      return static_cast <float> (random () * (static_cast <double> (high) - static_cast <double> (low)) / (m_divider - 1) + static_cast <double> (low));
   }
};

class Vector {
public:
   float x, y, z;

public:
   inline Vector (float scaler = 0.0f) : x (scaler), y (scaler), z (scaler) {}
   inline Vector (float inputX, float inputY, float inputZ) : x (inputX), y (inputY), z (inputZ) {}
   inline Vector (float *other) : x (other[0]), y (other[1]), z (other[2]) {}
   inline Vector (const Vector &right) : x (right.x), y (right.y), z (right.z) {}

public:
   inline operator float * (void) {
      return &x;
   }

   inline operator const float * (void) const {
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
      return fequal (x, right.x) && fequal (y, right.y) && fequal (z, right.z);
   }

   inline bool operator!= (const Vector &right) const {
      return !fequal (x, right.x) && !fequal (y, right.y) && !fequal (z, right.z);
   }

   inline const Vector &operator= (const Vector &right) {
      x = right.x;
      y = right.y;
      z = right.z;

      return *this;
   }

public:
   inline float length (void) const {
      return sqrtf (x * x + y * y + z * z);
   }

   inline float length2D (void) const {
      return sqrtf (x * x + y * y);
   }

   inline float lengthSq (void) const {
      return x * x + y * y + z * z;
   }

   inline Vector make2D (void) const {
      return Vector (x, y, 0.0f);
   }

   inline Vector normalize (void) const {
      float len = length () + static_cast <float> (FLEPSILON);

      if (fzero (len)) {
         return Vector (0.0f, 0.0f, 1.0f);
      }
      len = 1.0f / len;
      return Vector (x * len, y * len, z * len);
   }

   inline Vector normalize2D (void) const {
      float len = length2D () + static_cast <float> (FLEPSILON);

      if (fzero (len)) {
         return Vector (0.0f, 1.0f, 0.0f);
      }
      len = 1.0f / len;
      return Vector (x * len, y * len, 0.0f);
   }

   inline bool empty (void) const {
      return fzero (x) && fzero (y) && fzero (z);
   }

   inline static const Vector &null (void) {
      static const Vector s_zero = Vector (0.0f, 0.0f, 0.0f);
      return s_zero;
   }

   inline void nullify (void) {
      x = y = z = 0.0f;
   }

   inline Vector clampAngles (void) {
      x = angleNorm (x);
      y = angleNorm (y);
      z = 0.0f;

      return *this;
   }

   inline float toPitch (void) const {
      if (fzero (x) && fzero (y)) {
         return 0.0f;
      }
      return rad2deg (atan2f (z, length2D ()));
   }

   inline float toYaw (void) const {
      if (fzero (x) && fzero (y)) {
         return 0.0f;
      }
      return rad2deg (atan2f (y, x));
   }

   inline Vector toAngles (void) const {
      if (fzero (x) && fzero (y)) {
         return Vector (z > 0.0f ? 90.0f : 270.0f, 0.0, 0.0f);
      }
      return Vector (rad2deg (atan2f (z, length2D ())), rad2deg (atan2f (y, x)), 0.0f);
   }

   inline void makeVectors (Vector *forward, Vector *right, Vector *upward) const {
      float sinePitch = 0.0f, cosinePitch = 0.0f, sineYaw = 0.0f, cosineYaw = 0.0f, sineRoll = 0.0f, cosineRoll = 0.0f;

      sincosf (deg2rad (x), sinePitch, cosinePitch); // compute the sine and cosine of the pitch component
      sincosf (deg2rad (y), sineYaw, cosineYaw); // compute the sine and cosine of the yaw component
      sincosf (deg2rad (z), sineRoll, cosineRoll); // compute the sine and cosine of the roll component

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
         GetProcAddress (static_cast <HMODULE> (m_ptr), function);
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
   Pair (A a, B b) : first (a), second (b) {
   }

public:
   Pair (void) = default;
   ~Pair (void) = default;
};

template <typename T> class Array {
protected:
   T *m_data;
   size_t m_capacity, m_length;

public:
   Array (void) : m_data (nullptr), m_capacity (0), m_length (0) {
   }

   Array (Array &&other) noexcept {
      m_data = other.m_data;
      m_length = other.m_length;
      m_capacity = other.m_capacity;

      other.reset ();
   }

   virtual ~Array (void) {
      destroy ();
   }

public:
   void destroy (void) {
      delete[] m_data;
      reset ();
   }

   void reset (void) {
      m_data = nullptr;
      m_capacity = 0;
      m_length = 0;
   }

   bool reserve (size_t growSize) {
      if (m_length + growSize < m_capacity) {
         return true;
      }
      size_t maxSize = max <size_t> (m_capacity + 2, static_cast <size_t> (16));

      while (m_length + growSize > maxSize) {
         maxSize *= 2;
      }
      auto buffer = new T[maxSize];

      if (m_data != nullptr) {
         if (maxSize < m_length) {
            m_length = maxSize;
         }
         moveArray (buffer, m_data, m_length);
         delete[] m_data;
      }
      m_data = buffer;
      m_capacity = maxSize;

      return true;
   }

   bool resize (size_t newSize) {
      bool res = reserve (newSize);

      while (m_length < newSize) {
         push (T ());
      }
      return res;
   }

   inline size_t length (void) const {
      return m_length;
   }

   inline size_t capacity (void) const {
      return m_capacity;
   }

   bool set (size_t index, T object) {
      if (index >= m_capacity) {
         if (!reserve (index + 2)) {
            return false;
         }
      }
      m_data[index] = forward <T> (object);

      if (index >= m_length) {
         m_length = index + 1;
      }
      return true;
   }

   inline T &at (size_t index) {
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

      if (newSize >= m_capacity && !reserve (newSize)) {
         return false;
      }

      if (index >= m_length) {
         for (size_t i = 0; i < count; i++) {
            m_data[i + index] = forward <T> (objects[i]);
         }
         m_length = newSize;
      }
      else {
         size_t i = 0;

         for (i = m_length; i > index; i--) {
            m_data[i + count - 1] = move (m_data[i - 1]);
         }
         for (i = 0; i < count; i++) {
            m_data[i + index] = forward <T> (objects[i]);
         }
         m_length += count;
      }
      return true;
   }

   bool insert (size_t at, Array &other) {
      if (&other == this) {
         return false;
      }
      return insert (at, other.m_data, other.m_length);
   }

   bool erase (size_t index, size_t count) {
      if (index + count > m_capacity) {
         return false;
      }
      m_length -= count;

      for (size_t i = index; i < m_length; i++) {
         m_data[i] = move (m_data[i + count]);
      }
      return true;
   }

   bool shift (void) {
      return erase (0, 1);
   }

   bool unshift (T object) {
      return insert (0, &object);
   }

   bool erase (T &item) {
      return erase (index (item), 1);
   }

   bool push (T object) {
      return insert (m_length, &object);
   }

   bool push (T *objects, size_t count) {
      return insert (m_length, objects, count);
   }

   bool extend (Array &other) {
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
            moveArray (buffer, m_data);
         }
         delete[] m_data;
      }
      m_data = buffer;
      m_capacity = m_length;
   }

   size_t index (T &item) {
      return &item - &m_data[0];
   }

   T &pop (void) {
      T &item = m_data[m_length - 1];
      erase (m_length - 1, 1);

      return item;
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
      moveArray (m_data, other.m_data, other.m_length);
      m_length = other.m_length;

      return true;
   }

   void reverse (void) {
      for (size_t i = 0; i < m_length / 2; i++) {
         swap (m_data[i], m_data[m_length - 1 - i]);
      }
   }

   T &random (void) const {
      return m_data[RandomSequence::ref ().getInt (0, static_cast <int> (m_length - 1))];
   }

   Array &operator = (Array &&other) noexcept {
      destroy ();

      m_data = other.m_data;
      m_length = other.m_length;
      m_capacity = other.m_capacity;

      other.reset ();
      return *this;
   }

   T &operator [] (size_t index) {
      return at (index);
   }

   const T &operator [] (size_t index) const {
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

private:
   Array (const Array &) = delete;
   Array &operator = (const Array &) = delete;
};

template <typename A, typename B, size_t Capacity = 0> class BinaryHeap : private Array <Pair <A, B>> {
private:
   using PairType = Pair <A, B>;
   using Base = Array <PairType>;

   using Base::m_data;
   using Base::m_length;

public:
   BinaryHeap (void) {
      Base::reserve (Capacity);
   }
   BinaryHeap (BinaryHeap &&other) noexcept : Base (move (other)) { }

public:
   void push (const A &first, const B &second) {
      push ({ first, second });
   }

   void push (PairType &&pair) {
      Base::push (forward <PairType> (pair));

      if (length () > 1) {
         siftUp ();
      }
   }

   A pop (void) {
      assert (!empty ());

      if (length () == 1) {
         return Base::pop ().first;
      }
      PairType value = move (m_data[0]);

      m_data[0] = move (Base::back ());
      Base::pop ();

      siftDown ();
      return value.first;
   }

   inline bool empty (void) const {
      return !length ();
   }

   inline size_t length (void) const {
      return m_length;
   }

   inline void clear (void) {
      Base::clear ();
   }

   void siftUp (void) {
      size_t child = m_length - 1;

      while (child != 0) {
         size_t parentIndex = getParent (child);

         if (m_data[parentIndex].second <= m_data[child].second) {
            break;
         }
         swap (m_data[child], m_data[parentIndex]);
         child = parentIndex;
      }
   }

   void siftDown (void) {
      size_t parent = 0;
      size_t leftIndex = getLeft (parent);

      auto ref = move (m_data[parent]);

      while (leftIndex < m_length) {
         size_t rightIndex = getRight (parent);

         if (rightIndex < m_length) {
            if (m_data[rightIndex].second < m_data[leftIndex].second) {
               leftIndex = rightIndex;
            }
         }

         if (ref.second <= m_data[leftIndex].second) {
            break;
         }
         m_data[parent] = move (m_data[leftIndex]);

         parent = leftIndex;
         leftIndex = getLeft (parent);
      }
      m_data[parent] = move (ref);
   }

   BinaryHeap &operator = (BinaryHeap &&other) noexcept {
      Base::operator = (move (other));
      return *this;
   }

private:
   static constexpr size_t getLeft (size_t index) {
      return index << 1 | 1;
   }

   static constexpr size_t getRight (size_t index) {
      return ++index << 1;
   }

   static constexpr size_t getParent (size_t index) {
      return --index >> 1;
   }

private:
   BinaryHeap (const BinaryHeap &) = delete;
   BinaryHeap &operator = (const BinaryHeap &) = delete;
};

class String : private Array <char> {
public:
   static constexpr size_t INVALID_INDEX = static_cast <size_t> (-1);

private:
   using Base = Array <char>;

private:
   bool isTrimChar (char chr, const char *chars) {
      do {
         if (*chars == chr) {
            return true;
         }
         chars++;
      } while (*chars);
      return false;
   }

public:

   String (void) = default;
   String (String &&other) noexcept : Base (move (other)) { }
   ~String (void) = default;

public:
   String (const char chr) {
      assign (chr);
   }

   String (const char *str, size_t length = 0) {
      assign (str, length);
   }

   String (const String &str, size_t length = 0) : Base () {
      assign (str.chars (), length);
   }

public:
   String &assign (const char *str, size_t length = 0) {
      length = length > 0 ? length : strlen (str);

      clear ();
      resize (length);

      memcpy (m_data, str, length);
      terminate ();

      return *this;
   }

   String &assign (const String &str, size_t length = 0) {
      return assign (str.chars (), length);
   }

   String &assign (const char chr) {
      resize (2);
      m_data[0] = chr;

      return *this;
   }

   String &append (const char *str) {
      if (empty ()) {
         return assign (str);
      }
      const size_t maxLength = strlen (str) + 1;

      resize (length () + maxLength);
      strncat (m_data, str, maxLength);

      return *this;
   }

   String &append (const String &str) {
      return append (str.chars ());
   }

   String &append (const char chr) {
      const char app[] = { chr, '\0' };
      return append (app);
   }

   const char *chars (void) const {
      return empty () ? "" : m_data;
   }

   size_t length (void) const {
      return Base::length ();
   }

   bool empty (void) const {
      return !length ();
   }

   void clear (void) {
      Base::clear ();

      if (m_data) {
         m_data[0] = '\0';
      }
   }

   void erase (size_t index, size_t count = 1) {
      Base::erase (index, count);
      terminate ();
   }

   void reserve (size_t count) {
      Base::reserve (count);
   }

   void resize (size_t count) {
      Base::resize (count);
      terminate ();
   }

   int32 toInt32 (void) const {
      return atoi (chars ());
   }

   inline void terminate (void) {
      m_data[m_length] = '\0';
   }

   inline char &at (size_t index) {
      return m_data[index];
   }

   int32 compare (const String &what) const {
      return strcmp (m_data, what.begin ());
   }

   int32 compare (const char *what) const {
      return strcmp (begin (), what);
   }

   bool contains (const String &what) const {
      return strstr (m_data, what.begin ()) != nullptr;
   }

   template <size_t BufferSize = 512> void format (const char *fmt, ...) {
      va_list ap;
      char buffer[BufferSize];

      va_start (ap, fmt);
      vsnprintf (buffer, bufsize (buffer), fmt, ap);
      va_end (ap);

      assign (buffer);
   }

   template <size_t BufferSize = 512> void formatAppend (const char *fmt, ...) {
      va_list ap;
      char buffer[BufferSize];

      va_start (ap, fmt);
      vsnprintf (buffer, bufsize (buffer), fmt, ap);
      va_end (ap);

      append (buffer);
   }

   size_t insert (size_t at, const String &str) {
      if (Base::insert (at, str.begin (), str.length ())) {
         terminate ();
         return length ();
      }
      return 0;
   }

   size_t find (const String &search, size_t pos) const {
      if (pos > length ()) {
         return INVALID_INDEX;
      }
      size_t len = search.length ();

      for (size_t i = pos; len + i <= length (); i++) {
         if (strncmp (m_data + i, search.chars (), len) == 0) {
            return i;
         }
      }
      return INVALID_INDEX;
   }

   size_t replace (const String &what, const String &to) {
      if (!what.length () || !to.length ()) {
         return 0;
      }
      size_t numReplaced = 0, posIndex = 0;

      while (posIndex < length ()) {
         posIndex = find (what, posIndex);

         if (posIndex == INVALID_INDEX) {
            break;
         }
         erase (posIndex, what.length ());
         insert (posIndex, to);

         posIndex += to.length ();
         numReplaced++;
      }
      return numReplaced;
   }

   String substr (size_t start, size_t count = INVALID_INDEX) {
      String result;

      if (start >= length () || empty ()) {
         return result;
      }
      if (count == INVALID_INDEX) {
         count = length () - start;
      }
      else if (start + count >= length ()) {
         count = length () - start;
      }
      result.resize (count);

      moveArray (&result[0], &m_data[start], count);
      result[count] = '\0';

      return result;
   }

   char &operator[] (size_t index) {
      return at (index);
   }

   friend String operator + (const String &lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (const String &lhs, char rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (char lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (const String &lhs, const char *rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (const char *lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend bool operator == (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) == 0;
   }

   friend bool operator < (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) < 0;
   }

   friend bool operator > (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) > 0;
   }

   friend bool operator == (const char *lhs, const String &rhs) {
      return rhs.compare (lhs) == 0;
   }

   friend bool operator == (const String &lhs, const char *rhs) {
      return lhs.compare (rhs) == 0;
   }

   friend bool operator != (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) != 0;
   }

   friend bool operator != (const char *lhs, const String &rhs) {
      return rhs.compare (lhs) != 0;
   }

   friend bool operator != (const String &lhs, const char *rhs) {
      return lhs.compare (rhs) != 0;
   }

   String &operator = (const String &rhs) {
      return assign (rhs);
   }

   String &operator = (const char *rhs) {
      return assign (rhs);
   }

   String &operator = (char rhs) {
      return assign (rhs);
   }

   String &operator = (String &&other) noexcept {
      Base::operator = (move (other));
      return *this;
   }

   String &operator += (const String &rhs) {
      return append (rhs);
   }

   String &operator += (const char *rhs) {
      return append (rhs);
   }

   String &trimRight (const char *chars = "\r\n\t ") {
      if (empty ()) {
         return *this;
      }
      char *str = end () - 1;

      while (*str != 0) {
         if (isTrimChar (*str, chars)) {
            erase (str - begin ());
            str--;
         }
         else {
            break;
         }
      }
      return *this;
   }

   String &trimLeft (const char *chars = "\r\n\t ") {
      if (empty ()) {
         return *this;
      }
      char *str = begin ();

      while (isTrimChar (*str, chars)) {
         str++;
      }

      if (begin () != str) {
         erase (0, str - begin ());
      }
      return *this;
   }

   String &trim (const char *chars = "\r\n\t ") {
      return trimLeft (chars).trimRight (chars);
   }

   Array <String> split (const char *delimiter) {
      Array <String> tokens;
      size_t length, index = 0;

      do {
         index += strspn (&m_data[index], delimiter);
         length = strcspn (&m_data[index], delimiter);

         if (length > 0) {
            tokens.push (move (substr (index, length)));
         }
         index += length;
      } while (length > 0);

      return tokens;
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

template <typename K> struct StringHash {
   unsigned long operator () (const K &key) const {
      char *str = const_cast <char *> (key.chars ());
      size_t hash = key.length ();

      while (*str++) {
         hash = ((hash << 5) + hash) + *str;
      }
      return hash;
   }
};

template <typename K> struct IntHash {
   unsigned long operator () (K key) const {
      key = ((key >> 16) ^ key) * 0x119de1f3;
      key = ((key >> 16) ^ key) * 0x119de1f3;
      key = (key >> 16) ^ key;

      return key;
   }
};

template <typename K, typename V, typename H = StringHash <K>, size_t I = 256> class HashMap {
public:
   using Bucket = Pair <K, V>;

private:
   Array <Bucket> m_buckets[I];
   H m_hash;

   Array <Bucket> &getBucket (const K &key) {
      return m_buckets[m_hash (key) % I];
   }

public:
   HashMap (void) = default;
   ~HashMap (void) = default;

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
      getBucket (key).push (Pair <K, V> (key, val));
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
   V &operator [] (const K &key) {
      for (auto &entry : getBucket (key)) {
         if (entry.first == key) {
            return entry.second;
         }
      }
      static V ret;
      return ret;
   }

private:
   HashMap (const HashMap &) = delete;
   HashMap &operator = (const HashMap &) = delete;
};

class File {
private:
   FILE *m_handle;
   size_t m_size;

public:
   File (void) : m_handle (nullptr), m_size (0) {}

   File (const String &fileName, const String &mode = "rt") : m_handle (nullptr), m_size (0){
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
      return static_cast <char> (fputc (ch, m_handle));
   }

   bool puts (const String &buffer) {
      assert (m_handle != nullptr);

      if (fputs (buffer.chars (), m_handle) < 0) {
         return false;
      }
      return true;
   }

   size_t read (void *buffer, size_t size, size_t count = 1) {
      return fread (buffer, size, count, m_handle);
   }

   size_t write (void *buffer, size_t size, size_t count = 1) {
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

class MemoryLoader : public Singleton <MemoryLoader> {
private:
   using Load = uint8 * (*) (const char *, int *);
   using Unload = void (*) (void *);

private:
   Load m_load;
   Unload m_unload;

public:
   MemoryLoader (void) = default;
   ~MemoryLoader (void) = default;

public:
   void setup (Load load, Unload unload) {
      m_load = load;
      m_unload = unload;
   }

   uint8 *load (const char *name, int *size) {
      if (m_load) {
         return m_load (name, size);
      }
      return nullptr;
   }

   void unload (void *buffer) {
      if (m_unload) {
         m_unload (buffer);
      }
   }
};

class MemFile {
private:
   size_t m_size;
   size_t m_pos;
   uint8 *m_buffer;

public:
   MemFile (void) : m_size (0) , m_pos (0) , m_buffer (nullptr) {}

   MemFile (const String &filename) : m_size (0) , m_pos (0) , m_buffer (nullptr) {
      open (filename);
   }

   virtual ~MemFile (void) {
      close ();
   }

   bool open (const String &filename) {
      m_size = 0;
      m_pos = 0;
      m_buffer = MemoryLoader::ref ().load (filename.chars (), reinterpret_cast <int *> (&m_size));

      if (!m_buffer) {
         return false;
      }
      return true;
   }

   void close (void) {
      MemoryLoader::ref ().unload (m_buffer);

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

   size_t read (void *buffer, size_t size, size_t count = 1) {
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

   inline size_t getSize (void) const {
      return m_size;
   }

   bool isValid (void) const {
      return m_buffer && m_size > 0;
   }
};

}}


namespace cr {
namespace types {

using StringArray = cr::classes::Array <cr::classes::String>;
using IntArray = cr::classes::Array <int>;

}}


