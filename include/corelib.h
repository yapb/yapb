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

#include <platform.h>

#ifdef PLATFORM_WIN32
   #include <direct.h>
#else
   #include <sys/stat.h>
#endif
//
// Basic Types
//
using int8 = signed char;
using int16 = signed short;
using int32 = signed long;
using uint8 = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned long;

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

template <typename T, int N> constexpr int A_bufsize (const T (&)[N]) {
   return N - 1;
}

template <typename T, int N> constexpr int A_arrsize (const T (&)[N]) {
   return N;
}

namespace Math {
constexpr float MATH_ONEPSILON = 0.01f;
constexpr float MATH_EQEPSILON = 0.001f;
constexpr float MATH_FLEPSILON = 1.192092896e-07f;

constexpr float MATH_PI = 3.14159265358979323846f;
constexpr float MATH_PI_RECIPROCAL = 1.0f / MATH_PI;
constexpr float MATH_PI_HALF = MATH_PI * 0.5f;

constexpr float MATH_D2R = MATH_PI / 180.0f;
constexpr float MATH_R2D = 180.0f / MATH_PI;

constexpr float A_square (const float value) {
   return value * value;
}

template <typename T> constexpr T A_min (const T a, const T b) {
   return a < b ? a : b;
}

template <typename T> constexpr T A_max (const T a, const T b) {
   return a > b ? a : b;
}

template <typename T> constexpr T A_clamp (const T x, const T a, const T b) {
   return A_min (A_max (x, a), b);
}

template<typename T> constexpr T A_abs (const T a) {
   return a > 0 ? a : -a;
}

static inline float A_powf (const float a, const float b) {
   union {
      float d;
      int x;
   } res { a };

   res.x = static_cast <int> (b * (res.x - 1064866805) + 1064866805);
   return res.d;
}

static inline float A_sqrtf (const float value) {
   return A_powf (value, 0.5f);
}

static inline float A_sinf (const float value) {
   signed long sign = static_cast <signed long> (value * MATH_PI_RECIPROCAL);
   const float calc = (value - static_cast <float> (sign) * MATH_PI);

   const float square = A_square (calc);
   const float res = 1.00000000000000000000e+00f + square * (-1.66666671633720397949e-01f + square * (8.33333376795053482056e-03f + square * (-1.98412497411482036114e-04f +
      square * (2.75565571428160183132e-06f + square * (-2.50368472620721149724e-08f + square * (1.58849267073435385100e-10f + square * -6.58925550841432672300e-13f))))));

   return (sign & 1) ? -calc * res : value * res;
}

static inline float A_cosf (const float value) {
   signed long sign = static_cast <signed long> (value * MATH_PI_RECIPROCAL);
   const float calc = (value - static_cast <float> (sign) * MATH_PI);

   const float square = A_square (calc);
   const float res = square * (-5.00000000000000000000e-01f + square * (4.16666641831398010254e-02f + square * (-1.38888671062886714935e-03f + square * (2.48006890615215525031e-05f +
      square * (-2.75369927749125054106e-07f + square * (2.06207229069832465029e-09f + square * -9.77507137733812925262e-12f))))));

   const float f = -1.00000000000000000000e+00f;

   return (sign & 1) ? f - res : -f + res;
}

static inline float A_tanf (const float value) {
   return A_sinf (value) / A_cosf (value);
}

static inline float A_atan2f (const float y, const float x) {
   if (x == 0.0f) {
      if (y > 0.0f) {
         return MATH_PI_HALF;
      }
      else if (y < 0.0f) {
         return -MATH_PI_HALF;
      }
      return 0.0f;
   }
   float result = 0.0f;
   float z = y / x;

   if (A_abs (z) < 1.0f) {
      result = z / (1.0f + 0.28f * z * z);

      if (x < 0.0f) {
         if (y < 0.0f) {
            return result - MATH_PI;
         }
         return result + MATH_PI;
      }
   }
   else {
      result = MATH_PI_HALF - z / (z * z + 0.28f);

      if (y < 0.0f) {
         return result - MATH_PI;
      }
   }
   return result;
}

static inline void A_sincosf (const float rad, float &sine, float &cosine) {
   sine = A_sinf (rad);
   cosine = A_cosf (rad);
}

constexpr bool isFltZero (const float entry) {
   return A_abs (entry) < MATH_ONEPSILON;
}

constexpr bool isFltEqual (const float entry1, const float entry2) {
   return A_abs (entry1 - entry2) < MATH_EQEPSILON;
}

constexpr float radToDeg (const float radian) {
   return radian * MATH_R2D;
}

constexpr float degToRad (const float degree) {
   return degree * MATH_D2R;
}

constexpr float angleMod (const float angle) {
   return 360.0f / 65536.0f * (static_cast<int> (angle * (65536.0f / 360.0f)) & 65535);
}

constexpr float angleNorm (const float angle) {
   return 360.0f / 65536.0f * (static_cast<int> ((angle + 180.0f) * (65536.0f / 360.0f)) & 65535) - 180.0f;
}

constexpr float angleDiff (const float dest, const float src) {
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
      static constexpr unsigned int prime = 4294967291u;

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
      return Math::radToDeg (Math::A_atan2f (z, length2D ()));
   }

   inline float toYaw (void) const {
      if (Math::isFltZero (x) && Math::isFltZero (y)) {
         return 0.0f;
      }
      return Math::radToDeg (Math::A_atan2f (y, x));
   }

   inline Vector toAngles (void) const {
      if (Math::isFltZero (x) && Math::isFltZero (y)) {
         return Vector (z > 0.0f ? 90.0f : 270.0f, 0.0, 0.0f);
      }
      return Vector (Math::radToDeg (Math::A_atan2f (z, length2D ())), Math::radToDeg (Math::A_atan2f (y, x)), 0.0f);
   }

   inline void makeVectors (Vector *forward, Vector *right, Vector *upward) const {
      float sinePitch = 0.0f, cosinePitch = 0.0f, sineYaw = 0.0f, cosineYaw = 0.0f, sineRoll = 0.0f, cosineRoll = 0.0f;

      Math::A_sincosf (Math::degToRad (x), sinePitch, cosinePitch); // compute the sine and cosine of the pitch component
      Math::A_sincosf (Math::degToRad (y), sineYaw, cosineYaw); // compute the sine and cosine of the yaw component
      Math::A_sincosf (Math::degToRad (z), sineRoll, cosineRoll); // compute the sine and cosine of the roll component

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
   Array (void) : m_data (nullptr), m_capacity (0), m_length (0) {
   }

   Array (const Array &other) : m_data (nullptr), m_capacity (0), m_length (0) {
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

   bool reserve (size_t growSize) {
      if (m_length + growSize < m_capacity) {
         return true;
      }
      size_t maxSize = Math::A_max (m_capacity * 2, 8u);

      while (m_length + growSize > maxSize) {
         maxSize *= 2;
      }
      auto buffer = new T[maxSize];

      if (m_data != nullptr) {
         if (maxSize < m_length) {
            m_length = maxSize;
         }
         for (size_t i = 0; i < m_length; i++) {
            buffer[i] = m_data[i];
         }
         delete[] m_data;
      }
      m_data = buffer;
      m_capacity = maxSize;

      return true;
   }

   bool resize (size_t newSize) {
      m_length = newSize;
      return reserve (newSize);
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
      m_data[index] = object;

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

   void reverse (void) {
      for (size_t i = 0; i < m_length / 2; i++) {
         auto temp = m_data[i];
         m_data[i] = m_data[m_length - 1 - i];
         m_data[m_length - 1 - i] = temp;
      }
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
};


template <typename A, typename B, size_t Capacity = 0> class BinaryHeap {
private:
   using PairType = Pair <A, B>;

private:
   Array <PairType> m_heap;

public:
   BinaryHeap (void) {
      m_heap.reserve (Capacity);
   }
public:
   void push (const A &first, const B &second) {
      m_heap.push ({ first, second });

      if (m_heap.length () > 1) {
         siftUp ();
      }
   }

   A pop (void) {
      assert (!empty ());

      if (m_heap.length () == 1) {
         return m_heap.pop ().first;
      }
      auto value = m_heap[0];

      m_heap[0] = m_heap.back ();
      m_heap.pop ();

      siftDown ();
      return value.first;
   }

   inline bool empty (void) const {
      return !length ();
   }

   inline size_t length (void) const {
      return m_heap.length ();
   }

   inline void clear (void) {
      m_heap.clear ();
   }

private:
   void siftUp (void) {
      size_t child = m_heap.length () - 1;

      while (child != 0) {
         size_t parentIndex = getParent (child);

         if (m_heap[parentIndex].second <= m_heap[child].second) {
            break;
         }
         auto temp = m_heap[child];
         m_heap[child] = m_heap[parentIndex];
         m_heap[parentIndex] = temp;

         child = parentIndex;
      }
   }

   void siftDown (void) {
      size_t parent = 0;
      size_t leftIndex = getLeft (parent);

      auto ref = m_heap[parent];

      while (leftIndex < m_heap.length ()) {
         size_t rightIndex = getRight (parent);

         if (rightIndex < m_heap.length ()) {
            if (m_heap[rightIndex].second < m_heap[leftIndex].second) {
               leftIndex = rightIndex;
            }
         }

         if (ref.second <= m_heap[leftIndex].second) {
            break;
         }
         m_heap[parent] = m_heap[leftIndex];
         parent = leftIndex;

         leftIndex = getLeft (parent);
      }
      m_heap[parent] = ref;
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
};

class String {
public:
   static constexpr size_t INVALID_INDEX = static_cast <size_t> (-1);

private:
   Array <char> m_data;

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
   ~String (void) = default;

public:
   String (const char chr) {
      assign (chr);
   }

   String (const char *str, size_t length = 0) {
      assign (str, length);
   }

   String (const String &str, size_t length = 0) {
      assign (str.chars (), length);
   }

public:
   String &assign (const char *str, size_t length = 0) {
      length = length > 0 ? length : strlen (str);

      resize (length);
      memcpy (begin (), str, length);

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
      resize (length () + strlen (str));
      strcat (begin (), str);

      return *this;
   }

   String &append (const String &str) {
      return append (str.chars ());
   }

   String &append (const char chr) {
      char app[] = { chr, '\0' };
      return append (app);
   }

   const char *chars (void) const {
      if (empty ()) {
         return "";
      }
      return m_data.begin ();
   }

   size_t length (void) const {
      return m_data.length ();
   }

   bool empty (void) const {
      return !length ();
   }

   void clear (void) {
      m_data.clear ();
      m_data[0] = '\0';
   }

   void erase (size_t index, size_t count = 1) {
      m_data.erase (index, count);
      terminate ();
   }

   void reserve (size_t count) {
      m_data.reserve (count);
   }

   void resize (size_t count) {
      m_data.resize (count);
      terminate ();
   }

   int toInt32 (void) const {
      return atoi (chars ());
   }

   inline void terminate (void) {
      m_data[length ()] = '\0';
   }

   inline char &at (size_t index) {
      return m_data[index];
   }

   int compare (const String &what) const {
      return strcmp (m_data.begin (), what.begin ());
   }

   int compare (const char *what) const {
      return strcmp (begin (), what);
   }

   bool contains (const String &what) const {
      return strstr (m_data.begin (), what.begin ()) != nullptr;
   }

   template <size_t BufferSize = 512> void format (const char *fmt, ...) {
      va_list ap;
      char buffer[BufferSize];

      va_start (ap, fmt);
      vsnprintf (buffer, sizeof (buffer) - 1, fmt, ap);
      va_end (ap);

      assign (buffer);
   }

   template <size_t BufferSize = 512> void formatAppend (const char *fmt, ...) {
      va_list ap;
      char buffer[BufferSize];

      va_start (ap, fmt);
      vsnprintf (buffer, sizeof (buffer) - 1, fmt, ap);
      va_end (ap);

      append (buffer);
   }

   size_t insert (size_t at, const String &str) {
      if (m_data.insert (at, str.begin (), str.length ())) {
         terminate ();
         return length ();
      }
      return 0;
   }

   size_t find (const String &search, size_t pos) const {
      if (pos > length ()) {
         return INVALID_INDEX;
      }
      size_t length = search.length ();

      for (size_t i = pos; length + i <= m_data.length (); i++) {
         if (strncmp (m_data.begin () + i, search.chars (), length) == 0) {
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
      auto data = new char[length () + 1];
      size_t update = 0;

      for (size_t i = start; i < start + count; i++) {
         data[update++] = m_data[i];
      }
      data[update] = '\0';

      result.assign (data);
      delete[] data;

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

   String &operator += (const String &rhs) {
      return append (rhs);
   }

   String &operator += (const char *rhs) {
      return append (rhs);
   }

   char *begin (void) {
      return m_data.begin ();
   }

   char *begin (void) const {
      return m_data.begin ();
   }

   char *end (void) {
      return begin () + length ();
   }

   char *end (void) const {
      return begin () + length ();
   }

   String &trimRight (const char *chars = "\r\n\t ") {
      if (empty ()) {
         return *this;
      }
      char *str = end () - 1;

      while (*str != 0) {
         if (isTrimChar (*str, chars)) {
            erase (str - begin ());
         }
         else {
            break;
         }
         str--;
      }
      return *this;
   }

   String &trimLeft (const char *chars = "\r\n\t ") {
      char *str = begin ();

      while (isTrimChar (*str, chars))
         str++;

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
            tokens.push (substr (index, length));
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

template <typename K> struct MapKeyHash {
   unsigned long operator () (const K &key) const {
      char *str = const_cast <char *> (key.chars ());
      size_t hash = key.length ();

      while (*str++) {
         hash = ((hash << 5) + hash) + *str;
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

   Array<Bucket> &getBucket (const K &key) {
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
   V &operator [] (const K &key) {
      for (auto &entry : getBucket (key)) {
         if (entry.first == key) {
            return entry.second;
         }
      }
      assert (nullptr);
   }
};

class File {
private:
   FILE *m_handle;
   size_t m_size;

public:
   File (void) : m_handle (nullptr) , m_size (0) {}

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
      m_buffer = MemoryLoader::ref ().load (filename.chars (), reinterpret_cast<int *> (&m_size));

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

