//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.jeefo.net/license
//

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <ctype.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include <platform.h>

#ifdef PLATFORM_WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifdef ENABLE_SSE_INTRINSICS
#include <xmmintrin.h>
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

// Own min/max implementation
template <typename T> inline T A_min (T a, T b) { return a < b ? a : b; }
template <typename T> inline T A_max (T a, T b) { return a > b ? a : b; }
template <typename T> inline T A_clamp (T x, T a, T b) { return A_min (A_max (x, a), b); }

// Fast stricmp got somewhere from chromium
static inline int A_stricmp (const char *str1, const char *str2, int length = -1)
{
   int iter = 0;

   if (length == -1)
      length = strlen (str1);

   for (; iter < length; iter++)
   {
      if ((str1[iter] | 32) != (str2[iter] | 32))
         break;
   }

   if (iter != length)
      return 1;

   return 0;
}

// Cross platform strdup
static inline char *A_strdup (const char *str)
{
   return strcpy (new char[strlen (str) + 1], str);
}

// From metamod-p
static inline bool A_IsValidCodePointer (const void *ptr)
{
#ifdef PLATFORM_WIN32
   if (IsBadCodePtr (reinterpret_cast <FARPROC> (ptr)))
      return false;
#endif

   (void) (ptr);

   // do not check on linux
   return true;
}

#ifndef PLATFORM_WIN32
#define _unlink(p) unlink (p)
#define _mkdir(p) mkdir (p, 0777)
#endif

//
// Title: Utility Classes Header
//

//
// Function: Hash
//  Hash template for <Map>, <HashTable>
//
template <typename T> unsigned int Hash (const T &);

//
// Function: Hash
//  Hash for integer.
//
// Parameters:
//  tag - Value that should be hashed.
//
// Returns:
//  Hashed value.
//
template <typename T> unsigned int Hash (const int &tag)
{
   unsigned int key = (unsigned int) tag;

   key += ~(key << 16);
   key ^= (key >> 5);

   key += (key << 3);
   key ^= (key >> 13);

   key += ~(key << 9);
   key ^= (key >> 17);

   return key;
}

//
// Namespace: Math
//  Some math utility functions.
//
namespace Math
{
   const float MATH_ONEPSILON = 0.01f;
   const float MATH_EQEPSILON = 0.001f;
   const float MATH_FLEPSILON = 1.192092896e-07f;

   //
   // Constant: MATH_PI
   // Mathematical PI value.
   //
   const float MATH_PI = 3.141592653589793f;

   const float MATH_D2R = MATH_PI / 180.0f;
   const float MATH_R2D = 180.0f / MATH_PI;

#ifdef ENABLE_SSE_INTRINSICS
   //
   // Function: mm_abs
   //
   // mm version if abs
   //
   static inline __m128 mm_abs (__m128 val)
   {
      return _mm_andnot_ps (_mm_castsi128_ps (_mm_set1_epi32 (0x80000000)), val);
   };

   //
   // Function: mm_sine
   //
   // mm version if sine
   //
   static inline __m128 mm_sine (__m128 inp)
   {
      __m128 pi2 = _mm_set1_ps (MATH_PI * 2);
      __m128 val = _mm_cmpnlt_ps (inp, _mm_set1_ps (MATH_PI));

      val = _mm_and_ps (val, pi2);
      inp = _mm_sub_ps (inp, val);
      val = _mm_cmpngt_ps (inp, _mm_set1_ps (-MATH_PI));
      val = _mm_and_ps (val, pi2);
      inp = _mm_add_ps (inp, val);
      val = _mm_mul_ps (mm_abs (inp), _mm_set1_ps (-4.0f / (MATH_PI * MATH_PI)));
      val = _mm_add_ps (val, _mm_set1_ps (4.0f / MATH_PI));

      __m128 res = _mm_mul_ps (val, inp);

      val = _mm_mul_ps (mm_abs (res), res);
      val = _mm_sub_ps (val, res);
      val = _mm_mul_ps (val, _mm_set1_ps (0.225f));
      res = _mm_add_ps (val, res);

      return res;
   }
#endif
   //
   // Function: A_sqrtf
   //
   // SIMD version of sqrtf.
   //
   static inline float A_sqrtf (float value)
   {
#ifdef ENABLE_SSE_INTRINSICS
      return _mm_cvtss_f32 (_mm_sqrt_ss (_mm_load_ss (&value)));
#else
      return sqrtf (value);
#endif
   }

   //
   // Function: A_sinf
   //
   // SIMD version of sinf.
   //
   static inline float A_sinf (float value)
   {
#ifdef ENABLE_SSE_INTRINSICS
      return _mm_cvtss_f32 (mm_sine (_mm_set1_ps (value)));
#else
      return sinf (value);
#endif
   }

   //
   // Function: A_cosf
   //
   // SIMD version of cosf.
   //
   static inline float A_cosf (float value)
   {
#ifdef ENABLE_SSE_INTRINSICS
      return _mm_cvtss_f32 (mm_sine (_mm_set1_ps (value + MATH_PI / 2.0f)));
#else
      return cosf (value);
#endif
   }

   //
   // Function: A_sincosf
   //
   // SIMD version of sincosf.
   //
   static inline void A_sincosf (float rad, float *sine, float *cosine)
   {
#ifdef ENABLE_SSE_INTRINSICS
      __m128 m_sincos = mm_sine (_mm_set_ps (0.0f, 0.0f, rad + MATH_PI / 2.f, rad));
      __m128 m_cos = _mm_shuffle_ps (m_sincos, m_sincos, _MM_SHUFFLE (0, 0, 0, 1));

      *sine = _mm_cvtss_f32 (m_sincos);
      *cosine = _mm_cvtss_f32 (m_cos);
#else
      *sine = sinf (rad);
      *cosine = cosf (rad);
#endif
   }

   //
   // Function: FltZero
   // 
   // Checks whether input entry float is zero.
   //
   // Parameters:
   //   entry - Input float.
   //
   // Returns:
   //   True if float is zero, false otherwise.
   //
   // See Also:
   //   <FltEqual>
   //
   // Remarks:
   //   This eliminates Intel C++ Compiler's warning about float equality/inquality.
   //
   static inline bool FltZero (float entry)
   {
      return fabsf (entry) < MATH_ONEPSILON;
   }

   //
   // Function: FltEqual
   // 
   // Checks whether input floats are equal.
   //
   // Parameters:
   //   entry1 - First entry float.
   //   entry2 - Second entry float.
   //
   // Returns:
   //   True if floats are equal, false otherwise.
   //
   // See Also:
   //   <FltZero>
   //
   // Remarks:
   //   This eliminates Intel C++ Compiler's warning about float equality/inquality.
   //
   static inline bool FltEqual (float entry1, float entry2)
   {
      return fabsf (entry1 - entry2) < MATH_EQEPSILON;
   }

   //
   // Function: RadianToDegree
   // 
   //  Converts radians to degrees.
   //
   // Parameters:
   //   radian - Input radian.
   //
   // Returns:
   //   Degree converted from radian.
   //
   // See Also:
   //   <DegreeToRadian>
   //
   static inline float RadianToDegree (float radian)
   {
      return radian * MATH_R2D;
   }

   //
   // Function: DegreeToRadian
   // 
   // Converts degrees to radians.
   //
   // Parameters:
   //   degree - Input degree.
   //
   // Returns:
   //   Radian converted from degree.
   //
   // See Also:
   //   <RadianToDegree>
   //
   static inline float DegreeToRadian (float degree)
   {
      return degree * MATH_D2R;
   }

   //
   // Function: AngleMod
   //
   // Adds or subtracts 360.0f enough times need to given angle in order to set it into the range [0.0, 360.0f).
   //
   // Parameters:
   //     angle - Input angle.
   //
   // Returns:
   //   Resulting angle.
   //
   static inline float AngleMod (float angle)
   {
      return 360.0f / 65536.0f * (static_cast <int> (angle * (65536.0f / 360.0f)) & 65535);
   }

   //
   // Function: AngleNormalize
   //
   // Adds or subtracts 360.0f enough times need to given angle in order to set it into the range [-180.0, 180.0f).
   //
   // Parameters:
   //     angle - Input angle.
   //
   // Returns:
   //   Resulting angle.
   //
   static inline float AngleNormalize (float angle)
   {
      return 360.0f / 65536.0f * (static_cast <int> ((angle + 180.0f) * (65536.0f / 360.0f)) & 65535) - 180.0f;
   }


   //
   // Function: SineCosine
   //  Very fast platform-dependent sine and cosine calculation routine.
   //
   // Parameters:
   //  rad - Input degree.
   //  sin - Output for Sine.
   //  cos - Output for Cosine.
   //
   static inline void SineCosine (float rad, float *sine, float *cosine)
   {
#if defined (__ANDROID__)
      *sine = sinf (rad);
      *cosine = cosf (rad);
#else
      A_sincosf (rad, sine, cosine);
#endif
   }

   static inline float AngleDiff (float destAngle, float srcAngle)
   {
      return AngleNormalize (destAngle - srcAngle);
   }
}

//
// Class: RandomSequenceOfUnique
//  Random number generator used by the bot code.
//  See: https://github.com/preshing/RandomSequence/
//
class RandomSequenceOfUnique
{
private:
   unsigned int m_index;
   unsigned int m_intermediateOffset;
   unsigned long long m_divider;

private:
   unsigned int PermuteQPR (unsigned int x)
   {
      static const unsigned int prime = 4294967291u;

      if (x >= prime)
         return x;

      unsigned int residue = (static_cast <unsigned long long> (x)* x) % prime;

      return (x <= prime / 2) ? residue : prime - residue;
   }

   unsigned int Random (void)
   {
      return PermuteQPR ((PermuteQPR (m_index++) + m_intermediateOffset) ^ 0x5bf03635);
   }
public:
   RandomSequenceOfUnique (void)
   {
      unsigned int seedBase = static_cast <unsigned int> (time (NULL));
      unsigned int seedOffset = seedBase + 1;

      m_index = PermuteQPR (PermuteQPR (seedBase) + 0x682f0161);
      m_intermediateOffset = PermuteQPR (PermuteQPR (seedOffset) + 0x46790905);
      m_divider = (static_cast <unsigned long long> (1)) << 32;
   }

   inline int Int (int low, int high)
   {
      return static_cast <int> (Random () * (static_cast <double> (high) - static_cast <double> (low) + 1.0) / m_divider + static_cast <double> (low));
   }

   inline float Float (float low, float high)
   {
      return static_cast <float> (Random () * (static_cast <double> (high) - static_cast <double> (low)) / (m_divider - 1) + static_cast <double> (low));
   }
};

//
// Class: Vector
// Standard 3-dimensional vector.
//
class Vector
{
   //
   // Group: Variables.
   //
public:
   //
   // Variable: x,y,z
   // X, Y and Z axis members.
   //
   float x, y, z;

   //
   // Group: (Con/De)structors.
   //
public:
   //
   // Function: Vector
   //
   // Constructs Vector from float, and assign it's value to all axises.
   //
   // Parameters:
   //     scaler - Value for axises.
   //
   inline Vector (float scaler = 0.0f) : x (scaler), y (scaler), z (scaler)
   {
   }

   //
   // Function: Vector
   //
   // Standard Vector Constructor.
   //
   // Parameters:
   //     inputX - Input X axis.
   //     inputY - Input Y axis.
   //     inputZ - Input Z axis.
   //
   inline Vector (float inputX, float inputY, float inputZ) : x (inputX), y (inputY), z (inputZ)
   {
   }

   //
   // Function: Vector
   //
   // Constructs Vector from float pointer.
   //
   // Parameters:
   //     other - Float pointer.
   //
   inline Vector (float *other) : x (other[0]), y (other[1]), z (other[2])
   {
   }

   //
   // Function: Vector
   //
   // Constructs Vector from another Vector.
   //
   // Parameters:
   //   right - Other Vector, that should be assigned.
   //
   inline Vector (const Vector &right) : x (right.x), y (right.y), z (right.z)
   {
   }
   //
   // Group: Operators.
   //
public:
   inline operator float * (void)
   {
      return &x;
   }

   inline operator const float * (void) const
   {
      return &x;
   }


   inline const Vector operator + (const Vector &right) const
   {
      return Vector (x + right.x, y + right.y, z + right.z);
   }

   inline const Vector operator - (const Vector &right) const
   {
      return Vector (x - right.x, y - right.y, z - right.z);
   }

   inline const Vector operator - (void) const
   {
      return Vector (-x, -y, -z);
   }

   friend inline const Vector operator * (const float vec, const Vector &right)
   {
      return Vector (right.x * vec, right.y * vec, right.z * vec);
   }

   inline const Vector operator * (float vec) const
   {
      return Vector (vec * x, vec * y, vec * z);
   }

   inline const Vector operator / (float vec) const
   {
      const float inv = 1 / vec;
      return Vector (inv * x, inv * y, inv * z);
   }

   // cross product
   inline const Vector operator ^ (const Vector &right) const
   {
      return Vector (y * right.z - z * right.y, z * right.x - x * right.z, x * right.y - y * right.x);
   }

   // dot product
   inline float operator | (const Vector &right) const
   {
      return x * right.x + y * right.y + z * right.z;
   }

   inline const Vector &operator += (const Vector &right)
   {
      x += right.x;
      y += right.y;
      z += right.z;

      return *this;
   }

   inline const Vector &operator -= (const Vector &right)
   {
      x -= right.x;
      y -= right.y;
      z -= right.z;

      return *this;
   }

   inline const Vector &operator *= (float vec)
   {
      x *= vec;
      y *= vec;
      z *= vec;

      return *this;
   }

   inline const Vector &operator /= (float vec)
   {
      const float inv = 1 / vec;

      x *= inv;
      y *= inv;
      z *= inv;

      return *this;
   }

   inline bool operator == (const Vector &right) const
   {
      return Math::FltEqual (x, right.x) && Math::FltEqual (y, right.y) && Math::FltEqual (z, right.z);
   }

   inline bool operator != (const Vector &right) const
   {
      return !Math::FltEqual (x, right.x) && !Math::FltEqual (y, right.y) && !Math::FltEqual (z, right.z);
   }

   inline const Vector &operator = (const Vector &right)
   {
      x = right.x;
      y = right.y;
      z = right.z;

      return *this;
   }
   //
   // Group: Functions.
   //
public:
   //
   // Function: GetLength
   //
   // Gets length (magnitude) of 3D vector.
   //
   // Returns:
   //   Length (magnitude) of the 3D vector.
   //
   // See Also:
   //   <GetLengthSquared>
   //
   inline float GetLength (void) const
   {
      return Math::A_sqrtf (x * x + y * y + z * z);
   }

   //
   // Function: GetLength2D
   //
   // Gets length (magnitude) of vector ignoring Z axis.
   //
   // Returns:
   //   2D length (magnitude) of the vector.
   //
   // See Also:
   //   <GetLengthSquared2D>
   //
   inline float GetLength2D (void) const
   {
      return Math::A_sqrtf (x * x + y * y);
   }

   //
   // Function: GetLengthSquared
   //
   // Gets squared length (magnitude) of 3D vector.
   //
   // Returns:
   //   Squared length (magnitude) of the 3D vector.
   //
   // See Also:
   //   <GetLength>
   //
   inline float GetLengthSquared (void) const
   {
      return x * x + y * y + z * z;
   }

   //
   // Function: GetLengthSquared2D
   //
   // Gets squared length (magnitude) of vector ignoring Z axis.
   //
   // Returns:
   //   2D squared length (magnitude) of the vector.
   //
   // See Also:
   //   <GetLength2D>
   //
   inline float GetLengthSquared2D (void) const
   {
      return x * x + y * y;
   }

   //
   // Function: Get2D
   //
   // Gets vector without Z axis.
   //
   // Returns:
   //   2D vector from 3D vector.
   //
   inline Vector Get2D (void) const
   {
      return Vector (x, y, 0.0f);
   }

   //
   // Function: Normalize
   //
   // Normalizes a vector.
   //
   // Returns:
   //   The previous length of the 3D vector.
   //
   inline Vector Normalize (void) const
   {
      float length = GetLength () + static_cast <float> (Math::MATH_FLEPSILON);

      if (Math::FltZero (length))
         return Vector (0, 0, 1.0f);

      length = 1.0f / length;

      return Vector (x * length, y * length, z * length);
   }

   //
   // Function: Normalize
   //
   // Normalizes a 2D vector.
   //
   // Returns:
   //   The previous length of the 2D vector.
   //
   inline Vector Normalize2D (void) const
   {
      float length = GetLength2D () + static_cast <float> (Math::MATH_FLEPSILON);

      if (Math::FltZero (length))
         return Vector (0, 1.0, 0);

      length = 1.0f / length;

      return Vector (x * length, y * length, 0.0f);
   }

   //
   // Function: IsZero
   //
   // Checks whether vector is null.
   //
   // Returns:
   //   True if this vector is (0.0f, 0.0f, 0.0f) within tolerance, false otherwise.
   //
   inline bool IsZero (void) const
   {
      return Math::FltZero (x) && Math::FltZero (y) && Math::FltZero (z);
   }

   //
   // Function: GetNull
   //
   // Gets a nulled vector.
   //
   // Returns:
   //   Nulled vector.
   //
   inline static const Vector &GetZero (void)
   {
      static const Vector s_zero = Vector (0.0f, 0.0f, 0.0f);
      return s_zero;
   }

   inline void Zero (void)
   {
      x = 0.0f;
      y = 0.0f;
      z = 0.0f;
   }

   //
   // Function: ClampAngles
   //
   // Clamps the angles (ignore Z component).
   //
   // Returns:
   //   3D vector with clamped angles (ignore Z component).
   //
   inline Vector ClampAngles (void)
   {
      x = Math::AngleNormalize (x);
      y = Math::AngleNormalize (y);
      z = 0.0f;

      return *this;
   }

   //
   // Function: ToPitch
   //
   // Converts a spatial location determined by the vector passed into an absolute X angle (pitch) from the origin of the world.
   //
   // Returns:
   //   Pitch angle.
   //
   inline float ToPitch (void) const
   {
      if (Math::FltZero (x) && Math::FltZero (y))
         return 0.0f;

      return Math::RadianToDegree (atan2f (z, GetLength2D ()));
   }

   //
   // Function: ToYaw
   //
   // Converts a spatial location determined by the vector passed into an absolute Y angle (yaw) from the origin of the world.
   //
   // Returns:
   //   Yaw angle.
   //
   inline float ToYaw (void) const
   {
      if (Math::FltZero (x) && Math::FltZero (y))
         return 0.0f;

      return Math::RadianToDegree (atan2f (y, x));
   }

   //
   // Function: ToAngles
   //
   // Convert a spatial location determined by the vector passed in into constant absolute angles from the origin of the world.
   //
   // Returns:
   //   Converted from vector, constant angles.
   //
   inline Vector ToAngles (void) const
   {
      // is the input vector absolutely vertical?
      if (Math::FltZero (x) && Math::FltZero (y))
         return Vector (z > 0.0f ? 90.0f : 270.0f, 0.0, 0.0f);

      // else it's another sort of vector compute individually the pitch and yaw corresponding to this vector.
      return Vector (Math::RadianToDegree (atan2f (z, GetLength2D ())), Math::RadianToDegree (atan2f (y, x)), 0.0f);
   }

   //
   // Function: BuildVectors
   // 
   //  Builds a 3D referential from a view angle, that is to say, the relative "forward", "right" and "upward" direction 
   //  that a player would have if he were facing this view angle. World angles are stored in Vector structs too, the 
   //  "x" component corresponding to the X angle (horizontal angle), and the "y" component corresponding to the Y angle 
   //  (vertical angle).
   //
   // Parameters:
   //   forward - Forward referential vector.
   //   right - Right referential vector.
   //   upward - Upward referential vector.
   //
   inline void BuildVectors (Vector *forward, Vector *right, Vector *upward) const
   {
      float sinePitch = 0.0f, cosinePitch = 0.0f, sineYaw = 0.0f, cosineYaw = 0.0f, sineRoll = 0.0f, cosineRoll = 0.0f;

      Math::SineCosine (Math::DegreeToRadian (x), &sinePitch, &cosinePitch); // compute the sine and cosine of the pitch component
      Math::SineCosine (Math::DegreeToRadian (y), &sineYaw, &cosineYaw); // compute the sine and cosine of the yaw component
      Math::SineCosine (Math::DegreeToRadian (z), &sineRoll, &cosineRoll); // compute the sine and cosine of the roll component

      if (forward != NULL)
      {
         forward->x = cosinePitch * cosineYaw;
         forward->y = cosinePitch * sineYaw;
         forward->z = -sinePitch;
      }

      if (right != NULL)
      {
         right->x = -sineRoll * sinePitch * cosineYaw + cosineRoll * sineYaw;
         right->y = -sineRoll * sinePitch * sineYaw - cosineRoll * cosineYaw;
         right->z = -sineRoll * cosinePitch;
      }

      if (upward != NULL)
      {
         upward->x = cosineRoll * sinePitch * cosineYaw + sineRoll * sineYaw;
         upward->y = cosineRoll * sinePitch * sineYaw - sineRoll * cosineYaw;
         upward->z = cosineRoll * cosinePitch;
      }
   }
};

//
// Class: List
//  Simple linked list container.
//
template <typename T> class List
{
public:
   template <typename R> class Node
   {
   public:
      Node <R> *m_next;
      Node <R> *m_prev;
      T *m_data;

   public:
      Node (void)
      {
         m_next = NULL;
         m_prev = NULL;
         m_data = NULL;
      }
   };

private:
   Node <T> *m_first;
   Node <T> *m_last;
   int m_count;

//
// Group: (Con/De)structors
public:
   List (void)
   {
      Destory ();
   }

   virtual ~List (void) { };

//
// Group: Functions
//
public:

   //
   // Function: Destory
   //  Resets list to empty state by abandoning contents.
   //
   void Destory (void)
   {
      m_first = NULL;
      m_last = NULL;
      m_count = 0;
   }

   //
   // Function: GetSize
   //  Gets the number of elements in linked list.
   //
   // Returns:
   //  Number of elements in list.
   //
   inline int GetSize (void) const
   {
      return m_count;
   }

   //
   // Function: GetFirst
   //  Gets the first list entry. NULL in case list is empty.
   //
   // Returns:
   //  First list entry.
   //
   inline T *GetFirst (void) const
   {
      if (m_first != NULL)
         return m_first->m_data;

      return NULL;
   };

   //
   // Function: GetLast
   //  Gets the last list entry. NULL in case list is empty.
   //
   // Returns:
   //  Last list entry.
   //
   inline T *GetLast (void) const
   {
      if (m_last != NULL)
        return m_last->m_data;

      return NULL;
   };

   //
   // Function: GetNext
   //  Gets the next element from linked list.
   //
   // Parameters:
   //  current - Current node.
   //
   // Returns:
   //  Node data.
   //
   T *GetNext (T *current)
   {
      if (current == NULL)
         return GetFirst ();

      Node <T> *next = FindNode (current)->m_next;

      if (next != NULL)
         return next->m_data;

      return NULL;
   }

   //
   // Function: GetPrev
   //  Gets the previous element from linked list.
   //
   // Parameters:
   //  current - Current node.
   //
   // Returns:
   //  Node data.
   //
   T *GetPrev (T *current)
   {
      if (current == NULL)
         return GetLast ();

      Node <T> *prev = FindNode (current)->m_prev;

      if (prev != NULL)
         return prev->m_prev;

      return NULL;
   }

   //
   // Function: Link
   //  Adds item to linked list.
   //
   // Parameters:
   //  entry - Node that should be inserted in linked list.
   //  next - Next node to be inserted into linked list.
   //
   // Returns:
   //  Item if operation success, NULL otherwise.
   //
   T *Link (T *entry, T *next = NULL)
   {
      Node <T> *prevNode = NULL;
      Node <T> *nextNode = FindNode (next);
      Node <T> *newNode = new Node <T> ();

      newNode->m_data = entry;

      if (nextNode  == NULL)
      {
         prevNode = m_last;
         m_last = newNode;
      }
      else
      {
         prevNode = nextNode->m_prev;
         nextNode->m_prev = newNode;
      }

      if (prevNode == NULL)
         m_first = newNode;
      else
         prevNode->m_next = newNode;

      newNode->m_next = nextNode;
      newNode->m_prev = prevNode;

      m_count++;

      return entry;
   }

   //
   // Function: Link
   //  Adds item to linked list (as reference).
   //
   // Parameters:
   //  entry - Node that should be inserted in linked list.
   //  next - Next node to be inserted into linked list.
   //
   // Returns:
   //  Item if operation success, NULL otherwise.
   //
   T *Link (T &entry, T *next = NULL)
   {
      T *newEntry = new T ();
      *newEntry = entry;

      return Link (newEntry, next);
   }

   //
   // Function: Unlink
   //  Removes element from linked list.
   //
   // Parameters:
   //  entry - Element that should be moved out of list.  
   //
   void Unlink (T *entry)
   {
      Node <T> *entryNode = FindNode (entry);

      if (entryNode == NULL)
         return;

      if (entryNode->m_prev == NULL)
         m_first = entryNode->m_next;
      else
         entryNode->m_prev->m_next = entryNode->m_next;

      if (entryNode->m_next == NULL)
         m_last = entryNode->m_prev;
      else
         entryNode->m_next->m_prev = entryNode->m_prev;

      delete entryNode;
      m_count--;
   }

   //
   // Function: Allocate
   //  Inserts item into linked list, and allocating it automatically.
   //
   // Parameters:
   //  next - Optional next element.
   //
   // Returns:
   //  Item that was inserted.
   //
   T *Allocate (T *next = NULL)
   {
      T *entry = new T ();

      return Link (entry, next);
   }

   //
   // Function: Destory
   //  Removes element from list, and destroys it.
   //
   // Parameters:
   //  entry - Entry to perform operation on.
   //
   void Destory (T *entry)
   {
      Unlink (entry);
      delete entry;
   }

   //
   // Function: RemoveAll
   //  Removes all elements from list, and destroys them.
   //
   void RemoveAll (void)
   {
      Node <T> *node = NULL;

      while ((node = GetIterator (node)) != NULL)
      {
         Node <T> *nodeToKill = node;
         node = node->m_prev;

         free (nodeToKill->m_data);
      }
   }

   //
   // Function: FindNode
   //  Find node by it's entry.
   //
   // Parameters:
   //  entry - Entry to search.
   //
   // Returns:
   //  Node pointer.
   //
   Node <T> *FindNode (T *entry)
   {
      Node <T> *iter = NULL;

      while ((iter = GetIterator (iter)) != NULL)
      {
         if (iter->m_data == entry)
            return iter;
      }
      return NULL;
   }

   //
   // Function: GetIterator
   //  Utility node iterator.
   //
   // Parameters:
   //  entry - Previous entry.
   //
   // Returns:
   //  Node pointer.
   //
   Node <T> *GetIterator (Node <T> *entry = NULL)
   {
      if (entry == NULL)
         return m_first;
      
      return entry->m_next;
   }
};

//
// Class: Array
//  Universal template array container.
//
template <typename T> class Array
{
private:
   T *m_elements;

   int m_resizeStep;
   int m_itemSize;
   int m_itemCount;

//
// Group: (Con/De)structors
//
public:

   //
   // Function: Array
   //  Default array constructor.
   //
   // Parameters:
   //  resizeStep - Array resize step, when new items added, or old deleted.
   //
   Array (int resizeStep = 0)
   {
      m_elements = NULL;
      m_itemSize = 0;
      m_itemCount = 0;
      m_resizeStep = resizeStep;
   }

   //
   // Function: Array
   //  Array copying constructor.
   //
   // Parameters:
   //  other - Other array that should be assigned to this one.
   //
   Array (const Array <T> &other)
   {
      m_elements = NULL;
      m_itemSize = 0;
      m_itemCount = 0;
      m_resizeStep = 0;

      AssignFrom (other);
   }

   //
   // Function: ~Array
   //  Default array destructor.
   //
   virtual ~Array(void)
   {
      Destory ();
   }

//
// Group: Functions
//
public:

   //
   // Function: Destory
   //  Destroys array object, and all elements.
   //
   void Destory (void)
   {
      delete [] m_elements;

      m_elements = NULL;
      m_itemSize = 0;
      m_itemCount = 0;
   }

   //
   // Function: SetSize
   //  Sets the size of the array.
   //
   // Parameters:
   //  newSize - Size to what array should be resized.
   //  keepData - Keep exiting data, while resizing array or not.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool SetSize (int newSize, bool keepData = true)
   {
      if (newSize == 0)
      {
         Destory ();
         return true;
      }

      int checkSize = 0;

      if (m_resizeStep != 0)
         checkSize = m_itemCount + m_resizeStep;
      else
      {
         checkSize = m_itemCount / 8;

         if (checkSize < 4)
            checkSize = 4;

         if (checkSize > 1024)
            checkSize = 1024;

         checkSize += m_itemCount;
      }

      if (newSize > checkSize)
         checkSize = newSize;

      T *buffer = new T[checkSize];

      if (keepData && m_elements != NULL)
      {
         if (checkSize < m_itemCount)
            m_itemCount = checkSize;

         for (int i = 0; i < m_itemCount; i++)
            buffer[i] = m_elements[i];
      }
      delete [] m_elements;

      m_elements = buffer;
      m_itemSize = checkSize;

      return true;
   }

   //
   // Function: GetSize
   //  Gets allocated size of array.
   //
   // Returns:
   //  Number of allocated items.
   //
   int GetSize (void) const
   {
      return m_itemSize;
   }

   //
   // Function: GetElementNumber
   //  Gets real number currently in array.
   //
   // Returns:
   //  Number of elements.
   //
   int GetElementNumber (void) const
   {
      return m_itemCount;
   }

   //
   // Function: SetEnlargeStep
   //  Sets step, which used while resizing array data.
   //
   // Parameters:
   //  resizeStep - Step that should be set.
   //  
   void SetEnlargeStep (int resizeStep = 0)
   {
      m_resizeStep = resizeStep;
   }

   //
   // Function: GetEnlargeStep
   //  Gets the current enlarge step.
   //
   // Returns:
   //  Current resize step.
   //
   int GetEnlargeStep (void)
   {
      return m_resizeStep;
   }

   //
   // Function: SetAt
   //  Sets element data, at specified index.
   //
   // Parameters:
   //  index - Index where object should be assigned.
   //  object - Object that should be assigned.
   //  enlarge - Checks whether array must be resized in case, allocated size + enlarge step is exceeded.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool SetAt (int index, T object, bool enlarge = true)
   {
      if (index >= m_itemSize)
      {
         if (!enlarge || !SetSize (index + 1))
            return false;
      }
      m_elements[index] = object;

      if (index >= m_itemCount)
         m_itemCount = index + 1;

      return true;
   }

   //
   // Function: GetAt
   //  Gets element from specified index
   //
   // Parameters:
   //  index - Element index to retrieve.
   //
   // Returns:
   //  Element object.
   //
   T &GetAt (int index)
   {
      return m_elements[index];
   }

   //
   // Function: GetAt
   //  Gets element at specified index, and store it in reference object.
   //
   // Parameters:
   //  index - Element index to retrieve.
   //  object - Holder for element reference.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool GetAt (int index, T &object)
   {
      if (index >= m_itemCount)
         return false;

      object = m_elements[index];
      return true;
   }

   //
   // Function: InsertAt
   //  Inserts new element at specified index.
   //
   // Parameters:
   //  index - Index where element should be inserted.
   //  object - Object that should be inserted.
   //  enlarge - Checks whether array must be resized in case, allocated size + enlarge step is exceeded.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool InsertAt (int index, T object, bool enlarge = true)
   {
      return InsertAt (index, &object, 1, enlarge);
   }

   //
   // Function: InsertAt
   //  Inserts number of element at specified index.
   //
   // Parameters:
   //  index - Index where element should be inserted.
   //  objects - Pointer to object list.
   //  count - Number of element to insert.
   //  enlarge - Checks whether array must be resized in case, allocated size + enlarge step is exceeded.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool InsertAt (int index, T *objects, int count = 1, bool enlarge = true)
   {
      if (objects == NULL || count < 1)
         return false;

      int newSize = 0;

      if (m_itemCount > index)
         newSize = m_itemCount + count;
      else
         newSize = index + count;

      if (newSize >= m_itemSize)
      {
         if (!enlarge || !SetSize (newSize))
            return false;
      }

      if (index >= m_itemCount)
      {
         for (int i = 0; i < count; i++)
            m_elements[i + index] = objects[i];

         m_itemCount = newSize;
      }
      else
      {
         int i = 0;

         for (i = m_itemCount; i > index; i--)
            m_elements[i + count - 1] = m_elements[i - 1];

         for (i = 0; i < count; i++)
            m_elements[i + index] = objects[i];

         m_itemCount += count;
      }
      return true;
   }

   //
   // Function: InsertAt
   //  Inserts other array reference into the our array.
   //
   // Parameters:
   //  index - Index where element should be inserted.
   //  objects - Pointer to object list.
   //  count - Number of element to insert.
   //  enlarge - Checks whether array must be resized in case, allocated size + enlarge step is exceeded.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool InsertAt (int index, Array <T> &other, bool enlarge = true)
   {
      if (&other == this)
         return false;

      return InsertAt (index, other.m_elements, other.m_itemCount, enlarge);
   }

   //
   // Function: RemoveAt
   //  Removes elements from specified index.
   //
   // Parameters:
   //  index - Index, where element should be removed.
   //  count - Number of elements to remove.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool RemoveAt (int index, int count = 1)
   {
      if (index + count > m_itemCount)
         return false;

      if (count < 1)
         return true;

      m_itemCount -= count;

      for (int i = index; i < m_itemCount; i++)
         m_elements[i] = m_elements[i + count];

      return true;
   }

   //
   // Function: Push
   //  Appends element to the end of array.
   //
   // Parameters:
   //  object - Object to append.
   //  enlarge - Checks whether array must be resized in case, allocated size + enlarge step is exceeded.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool Push (T object, bool enlarge = true)
   {
      return InsertAt (m_itemCount, &object, 1, enlarge);
   }

   //
   // Function: Push
   //  Appends number of elements to the end of array.
   //
   // Parameters:
   //  objects - Pointer to object list.
   //  count - Number of element to insert.
   //  enlarge - Checks whether array must be resized in case, allocated size + enlarge step is exceeded.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool Push (T *objects, int count = 1, bool enlarge = true)
   {
      return InsertAt (m_itemCount, objects, count, enlarge);
   }

   //
   // Function: Push
   //  Inserts other array reference into the our array.
   //
   // Parameters:
   //  objects - Pointer to object list.
   //  count - Number of element to insert.
   //  enlarge - Checks whether array must be resized in case, allocated size + enlarge step is exceeded.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool Push (Array <T> &other, bool enlarge = true)
   {
      if (&other == this)
         return false;

      return InsertAt (m_itemCount, other.m_elements, other.m_itemCount, enlarge);
   }

   //
   // Function: GetData
   //  Gets the pointer to all element in array.
   //
   // Returns:
   //  Pointer to object list.
   //
   T *GetData (void)
   {
      return m_elements;
   }

   //
   // Function: RemoveAll
   //  Resets array, and removes all elements out of it.
   // 
   void RemoveAll (void)
   {
      m_itemCount = 0;
      SetSize (m_itemCount);
   }

   //
   // Function: IsEmpty
   //  Checks whether element is empty.
   //
   // Returns:
   //  True if element is empty, false otherwise.
   //
   inline bool IsEmpty (void)
   {
      return m_itemCount <= 0;
   }

   //
   // Function: FreeExtra
   //  Frees unused space.
   //
   void FreeSpace (bool destroyIfEmpty = true)
   {
      if (m_itemCount == 0)
      {
         if (destroyIfEmpty)
            Destory ();

         return;
      }

      T *buffer = new T[m_itemCount];

      if (m_elements != NULL)
      {
         for (int i = 0; i < m_itemCount; i++)
            buffer[i] = m_elements[i];
      }
      delete [] m_elements;

      m_elements = buffer;
      m_itemSize = m_itemCount;
   }

   //
   // Function: Pop
   //  Pops element from array.
   //
   // Returns:
   //  Object popped from the end of array.
   //
   T Pop (void)
   {
      T element = m_elements[m_itemCount - 1];
      RemoveAt (m_itemCount - 1);

      return element;
   }

   T &Last (void)
   {
      return m_elements[m_itemCount - 1];
   }

   bool GetLast (T &item)
   {
      if (m_itemCount <= 0)
         return false;

      item = m_elements[m_itemCount - 1];

      return true;
   }

   //
   // Function: AssignFrom
   //  Reassigns current array with specified one.
   //
   // Parameters:
   //  other - Other array that should be assigned.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool AssignFrom (const Array <T> &other)
   {
      if (&other == this)
         return true;

      if (!SetSize (other.m_itemCount, false))
         return false;

      for (int i = 0; i < other.m_itemCount; i++)
         m_elements[i] = other.m_elements[i];

      m_itemCount = other.m_itemCount;
      m_resizeStep = other.m_resizeStep;

      return true;
   }

   //
   // Function: GetRandomElement
   //  Gets the random element from the array.
   //
   // Returns:
   //  Random element reference.
   //
   T &GetRandomElement (void) const
   {
      extern class RandomSequenceOfUnique Random;

      return m_elements[Random.Int (0, m_itemCount - 1)];
   }

   Array <T> &operator = (const Array <T> &other)
   {
      AssignFrom (other);
      return *this;
   }

   T &operator [] (int index)
   {
      if (index < m_itemSize && index >= m_itemCount)
         m_itemCount = index + 1;

      return GetAt (index);
   }
};

//
// Class: Map
//  Represents associative map container.
//
template <class K, class V> class Map
{
public:
   struct MapItem
   {
      K key;
      V value;
   };

protected:
   struct HashItem
   {
   public:
      int m_index;
      HashItem *m_next;

   public:
      HashItem (void) 
      { 
         m_next = NULL;
         m_index = 0;
      }

      HashItem (int index, HashItem *next) 
      { 
         m_index = index;  
         m_next = next; 
      }
   };

   int m_hashSize;
   HashItem **m_table;
   Array <MapItem> m_mapTable;

// Group: (Con/De)structors
public:

   //
   // Function: Map
   //  Default constructor for map container.
   //
   // Parameters:
   //  hashSize - Initial hash size.
   //
   Map (int hashSize = 36)
   {
      m_hashSize = hashSize;
      m_table = new HashItem *[hashSize];

      for (int i = 0; i < hashSize; i++)
         m_table[i] = 0;
   }

   //
   // Function: ~Map
   //  Default map container destructor.
   //
   // Parameters:
   //  hashSize - Initial hash size.
   //
   virtual ~Map (void)
   {
      RemoveAll ();
      delete [] m_table;
   }

// Group: Functions
public:

   //
   // Function: IsExists
   //  Checks whether specified element exists in container.
   //
   // Parameters:
   //  keyName - Key that should be looked up.
   //
   // Returns:
   //  True if key exists, false otherwise.
   //
   inline bool IsExists (const K &keyName) 
   { 
      return GetIndex (keyName, false) >= 0; 
   }

   //
   // Function: SetupMap
   //  Initializes map, if not initialized automatically.
   //
   // Parameters:
   //  hashSize - Initial hash size.
   //
   inline void SetupMap (int hashSize)
   {
      m_hashSize = hashSize;
      m_table = new HashItem *[hashSize];

      for (int i = 0; i < hashSize; i++)
         m_table[i] = 0;
   }

   //
   // Function: IsEmpty
   //  Checks whether map container is currently empty.
   //
   // Returns:
   //  True if no elements exists, false otherwise.
   //
   inline bool IsEmpty (void) const 
   { 
      return m_mapTable.GetSize () == 0; 
   }

   //
   // Function: GetSize
   //  Retrieves size of the map container.
   //
   // Returns:
   //  Number of elements currently in map container.
   //
   inline int GetSize (void) const 
   { 
      return m_mapTable.GetSize (); 
   }

   //
   // Function: GetKey
   //  Gets the key object, by it's index.
   //
   // Parameters:
   //  index - Index of key.
   //
   // Returns:
   //  Object containing the key.
   //
   inline const K &GetKey (int index) const 
   { 
      return m_mapTable[index].key; 
   }

   //
   // Function: GetValue
   //  Gets the constant value object, by it's index.
   //
   // Parameters:
   //  index - Index of value.
   //
   // Returns:
   //  Object containing the value.
   //
   inline const V &GetValue (int index) const 
   {
      return m_mapTable[index].value; 
   }

   // Function: GetValue
   //  Gets the value object, by it's index.
   //
   // Parameters:
   //  index - Index of value.
   //
   // Returns:
   //  Object containing the value.
   //
   inline V &GetValue (int index) 
   { 
      return m_mapTable[index].value; 
   }

   //
   // Function: GetElements
   //  Gets the all elements of container.
   //
   // Returns:
   //  Array of elements, containing inside container.
   //
   // See Also:
   //  <Array>
   //
   inline Array <MapItem> &GetElements (void) 
   { 
      return m_mapTable; 
   }

   V &operator [] (const K &keyName)
   {
      int index = GetIndex (keyName, true);
      return m_mapTable[index].value;
   }

   //
   // Function: Find
   //  Finds element by his key name.
   //
   // Parameters:
   //  keyName - Key name to be searched.
   //  element - Holder for element object.
   //
   // Returns:
   //  True if element found, false otherwise.
   //
   bool Find (const K &keyName, V &element) const
   {
      int index = const_cast <Map <K, V> *> (this)->GetIndex (keyName, false);

      if (index < 0)
         return false;

      element = m_mapTable[index].value;
      return true;
   }

   //
   // Function: Find
   //  Finds element by his key name.
   //
   // Parameters:
   //  keyName - Key name to be searched.
   //  elementPtr - Holder for element pointer.
   //
   // Returns:
   //  True if element found, false otherwise.
   //
   bool Find (const K &keyName, V *&elementPtr) const
   {
      int index = const_cast <Map <K, V> *> (this)->GetIndex (keyName, false);

      if (index < 0)
         return false;

      elementPtr = const_cast <V *> (&m_mapTable[index].value);
      return true;
   }

   //
   // Function: Remove
   //  Removes element from container.
   //
   // Parameters:
   //  keyName - Key name of element, that should be removed.
   //
   // Returns:
   //  True if key was removed successfully, false otherwise.
   //
   bool Remove (const K &keyName)
   {
      int hashId = Hash <K> (keyName) % m_hashSize;
      HashItem *hashItem = m_table[hashId], *nextHash = 0;

      while (hashItem != NULL)
      {
         if (m_mapTable[hashItem->m_index].key == keyName)
         {
            if (nextHash == 0)
               m_table[hashId] = hashItem->m_next;
            else
               nextHash->m_next = hashItem->m_next;

            m_mapTable.RemoveAt (hashItem->m_index);
            delete hashItem;

            return true;
         }
         nextHash = hashItem;
         hashItem = hashItem->m_next;
      }
      return false;
   }

   //
   // Function: RemoveAll
   //  Removes all elements from container.
   //
   void RemoveAll (void)
   {
      for (int i = m_hashSize; --i >= 0;)
      {
         HashItem *ptr = m_table[i];

         while (ptr != NULL)
         {
            HashItem *m_next = ptr->m_next;

            delete ptr;
            ptr = m_next;
         }
         m_table[i] = 0;
      }
      m_mapTable.RemoveAll ();
   }

   //
   // Function: GetIndex
   //  Gets index of element.
   //
   // Parameters:
   //  keyName - Key of element.
   //  create - If true and no element found by a keyName, create new element.
   //
   // Returns:
   //  Either found index, created index, or -1 in case of error.
   //
   int GetIndex (const K &keyName, bool create)
   {
      int hashId = Hash <K> (keyName) % m_hashSize;

      for (HashItem *ptr = m_table[hashId]; ptr != NULL; ptr = ptr->m_next)
      {
         if (m_mapTable[ptr->m_index].key == keyName)
            return ptr->m_index;
      }

      if (create)
      {
         int item = m_mapTable.GetSize ();
         m_mapTable.SetSize (static_cast <int> (item + 1));

         m_table[hashId] = new HashItem (item, m_table[hashId]);
         m_mapTable[item].key = keyName;

         return item;
      }
      return -1;
   }
};

//
// Class: String
//  Reference counted string class.
//
class String
{
private:
   char *m_bufferPtr;
   int m_allocatedSize;
   int m_stringLength;

//
// Group: Private functions
//
private:

   //
   // Function: UpdateBufferSize
   //  Updates the buffer size.
   //
   // Parameters:
   //  size - New size of buffer.
   //
   void UpdateBufferSize (int size)
   {
      if (size <= m_allocatedSize)
         return;

      m_allocatedSize = size + 16;
      char *tempBuffer = new char[size + 1];

      if (m_bufferPtr != NULL)
      {
         strcpy (tempBuffer, m_bufferPtr);
         tempBuffer[m_stringLength] = 0;

         delete [] m_bufferPtr;
      }
      m_bufferPtr = tempBuffer;
      m_allocatedSize = size;
   }

   //
   // Function: MoveItems
   //  Moves characters inside buffer pointer.
   //
   // Parameters:
   //  destIndex - Destination index.
   //  sourceIndex - Source index.
   //
   void MoveItems (int destIndex, int sourceIndex)
   {
      memmove (m_bufferPtr + destIndex, m_bufferPtr + sourceIndex, sizeof (char) *(m_stringLength - sourceIndex + 1));
   }

   //
   // Function: Initialize
   //  Initializes string buffer.
   //
   // Parameters:
   //  length - Initial length of string.
   //
   void Initialize (int length)
   {
      int freeSize = m_allocatedSize - m_stringLength - 1;

      if (length <= freeSize)
         return;

      int delta = 4;

      if (m_allocatedSize > 64)
         delta = static_cast <int> (m_allocatedSize * 0.5);
      else if (m_allocatedSize > 8)
         delta = 16;

      if (freeSize + delta < length)
         delta = length - freeSize;

      UpdateBufferSize (m_allocatedSize + delta);
   }

   //
   // Function: CorrectIndex
   //  Gets the correct string end index.
   //
   // Parameters:
   //  index - Holder for index.
   //
   void CorrectIndex (int &index) const
   {
      if (index > m_stringLength)
         index = m_stringLength;
   }

   //
   // Function: InsertSpace
   //  Inserts space at specified location, with specified length.
   //
   // Parameters:
   //  index - Location to insert space.
   //  size - Size of space insert.
   //
   void InsertSpace (int &index, int size)
   {
      CorrectIndex (index);
      Initialize (size);

      MoveItems (index + size, index);
   }

   //
   // Function: IsTrimChar
   //  Checks whether input is trimming character.
   //
   // Parameters:
   //  input - Input to check for.
   //
   // Returns:
   //  True if it's a trim char, false otherwise.
   //
   bool IsTrimChar (char input)
   {
      return input == ' ' || input == '\t' || input == '\n';
   }

//
// Group: (Con/De)structors
//
public:
   String (void)
   {
      m_bufferPtr = NULL;
      m_allocatedSize = 0;
      m_stringLength = 0;
   }

   ~String (void)
   {
      delete [] m_bufferPtr;
   }

   String (const char *bufferPtr)
   {
      m_bufferPtr = NULL;
      m_allocatedSize = 0;
      m_stringLength = 0;

      Assign (bufferPtr);
   }

   String (char input)
   {
      m_bufferPtr = NULL;
      m_allocatedSize = 0;
      m_stringLength = 0;

      Assign (input);
   }

   String (const String &inputString)
   {
      m_bufferPtr = NULL;
      m_allocatedSize = 0;
      m_stringLength = 0;

      Assign (inputString.GetBuffer ());
   }

//
// Group: Functions
//
public:

   //
   // Function: GetBuffer
   //  Gets the string buffer.
   //
   // Returns:
   //  Pointer to buffer.
   //
   const char *GetBuffer (void)
   {
      if (m_bufferPtr == NULL || *m_bufferPtr == 0x0)
         return "";

      return &m_bufferPtr[0];
   }

   //
   // Function: GetBuffer
   //  Gets the string buffer (constant).
   //
   // Returns:
   //  Pointer to constant buffer.
   //
   const char *GetBuffer (void) const
   {
      if (m_bufferPtr == NULL || *m_bufferPtr == 0x0)
         return "";

      return &m_bufferPtr[0];
   }
   
   //
   // Function: ToFloat
   //  Gets the string as float, if possible.
   //
   // Returns:
   //  Float value of string.
   //
   float ToFloat (void)
   {
      return static_cast <float> (atof (m_bufferPtr));
   }

   //
   // Function: ToInt
   //  Gets the string as integer, if possible.
   //
   // Returns:
   //  Integer value of string.
   //
   int ToInt (void) const
   {
      return atoi (m_bufferPtr);
   }

   //
   // Function: ReleaseBuffer
   //  Terminates the string with null character.
   //
   void ReleaseBuffer (void)
   {
      ReleaseBuffer (strlen (m_bufferPtr));
   }

   //
   // Function: ReleaseBuffer
   //  Terminates the string with null character with specified buffer end.
   //
   // Parameters:
   //  newLength - End of buffer.
   //
   void ReleaseBuffer (int newLength)
   {
      m_bufferPtr[newLength] = 0;
      m_stringLength = newLength;
   }

   //
   // Function: GetBuffer
   //  Gets the buffer with specified length.
   //
   // Parameters:
   //  minLength - Length to retrieve.
   //
   // Returns:
   //  Pointer to string buffer.
   //
   char *GetBuffer (int minLength)
   {
      if (minLength >= m_allocatedSize)
         UpdateBufferSize (minLength + 1);

      return m_bufferPtr;
   }

   //
   // Function: GetBufferSetLength
   //  Gets the buffer with specified length, and terminates string with that length.
   //
   // Parameters:
   //  minLength - Length to retrieve.
   //
   // Returns:
   //  Pointer to string buffer.
   //
   char *GetBufferSetLength (int length)
   {
      char *buffer = GetBuffer (length);

      m_stringLength = length;
      m_bufferPtr[length] = 0;

      return buffer;
   }

   //
   // Function: Append
   //  Appends the string to existing buffer.
   //
   // Parameters:
   //  bufferPtr - String buffer to append.
   //
   void Append (const char *bufferPtr)
   {
      UpdateBufferSize (m_stringLength + strlen (bufferPtr) + 1);
      strcat (m_bufferPtr, bufferPtr);

      m_stringLength = strlen (m_bufferPtr);
   }

   //
   // Function: Append
   //  Appends the character to existing buffer.
   //
   // Parameters:
   //  input - Character to append.
   //
   void Append (const char input)
   {
      UpdateBufferSize (m_stringLength + 2);

      m_bufferPtr[m_stringLength] = input;
      m_bufferPtr[m_stringLength++] = 0;
   }

   //
   // Function: Append
   //  Appends the string to existing buffer.
   //
   // Parameters:
   //  inputString - String buffer to append.
   //
   void Append (const String &inputString)
   {
      const char *bufferPtr = inputString.GetBuffer ();
      UpdateBufferSize (m_stringLength + strlen (bufferPtr));

      strcat (m_bufferPtr, bufferPtr);
      m_stringLength = strlen (m_bufferPtr);
   }

   //
   // Function: AppendFormat
   //  Appends the formatted string to existing buffer.
   //
   // Parameters:
   //  fmt - Formatted, tring buffer to append.
   //
   void AppendFormat (const char *fmt, ...)
   {
      va_list ap;
      char buffer[1024];

      va_start (ap, fmt);
      vsnprintf (buffer, sizeof (buffer) - 1, fmt, ap);
      va_end (ap);

      Append (buffer);
   }

   //
   // Function: Assign
   //  Assigns the string to existing buffer.
   //
   // Parameters:
   //  inputString - String buffer to assign.
   //
   void Assign (const String &inputString)
   {
      Assign (inputString.GetBuffer ());
   }

   //
   // Function: Assign
   //  Assigns the character to existing buffer.
   //
   // Parameters:
   //  input - Character to assign.
   //
   void Assign (char input)
   {
      char psz[2] = {input, 0};
      Assign (psz);
   }

   //
   // Function: Assign
   //  Assigns the string to existing buffer.
   //
   // Parameters:
   //  bufferPtr - String buffer to assign.
   //
   void Assign (const char *bufferPtr)
   {
      if (bufferPtr == NULL)
      {
         UpdateBufferSize (1);
         m_stringLength = 0;

         return;
      }
      UpdateBufferSize (strlen (bufferPtr));

      if (m_bufferPtr != NULL)
      {
         strcpy (m_bufferPtr, bufferPtr);
         m_stringLength = strlen (m_bufferPtr);
      }
      else
         m_stringLength = 0;
   }

   //
   // Function: Assign
   //  Assigns the formatted string to existing buffer.
   //
   // Parameters:
   //  fmt - Formatted string buffer to assign.
   //
   void AssignFormat (const char *fmt, ...)
   {
      va_list ap;
      char buffer[1024];

      va_start (ap, fmt);
      vsnprintf (buffer, sizeof (buffer) - 1, fmt, ap);
      va_end (ap);

      Assign (buffer);
   }

   //
   // Function: Empty
   //  Empties the string.
   //
   void Empty (void)
   {
      if (m_bufferPtr != NULL)
      {
         m_bufferPtr[0] = 0;
         m_stringLength = 0;
      }
   }

   //
   // Function: IsEmpty
   //  Checks whether string is empty.
   //
   // Returns:
   //  True if string is empty, false otherwise.
   //
   bool IsEmpty (void) const
   {
      if (m_bufferPtr == NULL || m_stringLength == 0)
         return true;

      return false;
   }

   //
   // Function: GetLength
   //  Gets the string length.
   //
   // Returns:
   //  Length of string, 0 in case of error.
   //
   int GetLength (void) const
   {
      if (m_bufferPtr == NULL)
         return 0;

      return m_stringLength;
   }

   operator const char * (void) const
   {
      return GetBuffer ();
   }

   operator char * (void)
   {
      return const_cast <char *> (GetBuffer ());
   }

   operator int (void)
   {
      return ToInt ();
   }

   operator long (void)
   {
      return static_cast <long> (ToInt ());
   }

   operator float (void)
   {
      return ToFloat ();
   }

   operator double (void)
   {
      return static_cast <double> (ToFloat ());
   }

   friend String operator + (const String &s1, const String &s2)
   {
      String result (s1); 
      result += s2; 
      
      return result;
   }

   friend String operator + (const String &holder, char ch)
   {
      String result (holder); 
      result += ch; 
      
      return result;
   }

   friend String operator + (char ch, const String &holder)
   {
      String result (ch);
      result += holder;
      
      return result;
   }

   friend String operator + (const String &holder, const char *str)
   {
      String result (holder);
      result += str;
      
      return result;
   }

   friend String operator + (const char *str, const String &holder)
   {
      String result (const_cast <char *> (str));
      result += holder;
      
      return result;
   }

   friend bool operator == (const String &s1, const String &s2)
   {
      return s1.Compare (s2) == 0;
   }

   friend bool operator < (const String &s1, const String &s2)
   {
      return s1.Compare (s2) < 0;
   }

   friend bool operator > (const String &s1, const String &s2)
   {
      return s1.Compare (s2) > 0;
   }

   friend bool operator == (const char *s1, const String &s2)
   {
      return s2.Compare (s1) == 0;
   }

   friend bool operator == (const String &s1, const char *s2)
   {
      return s1.Compare (s2) == 0;
   }

   friend bool operator != (const String &s1, const String &s2)
   {
      return s1.Compare (s2) != 0;
   }

   friend bool operator != (const char *s1, const String &s2)
   {
      return s2.Compare (s1) != 0;
   }

   friend bool operator != (const String &s1, const char *s2)
   {
      return s1.Compare (s2) != 0;
   }

   String &operator = (const String &inputString)
   {
      Assign (inputString);
      return *this;
   }

   String &operator = (const char *bufferPtr)
   {
      Assign (bufferPtr);
      return *this;
   }

   String &operator = (char input)
   {
      Assign (input);
      return *this;
   }

   String &operator += (const String &inputString)
   {
      Append (inputString);
      return *this;
   }

   String &operator += (const char *bufferPtr)
   {
      Append (bufferPtr);
      return *this;
   }

   char operator [] (int index)
   {
      if (index > m_stringLength)
         return -1;

      return m_bufferPtr[index];
   }

   //
   // Function: Mid
   //  Gets the substring by specified bounds.
   //
   // Parameters:
   //  startIndex - Start index to get from.
   //  count - Number of characters to get.
   //
   // Returns:
   //  Tokenized string.
   //
   String Mid (int startIndex, int count = -1)
   {
      String result;

      if (startIndex >= m_stringLength || !m_bufferPtr)
         return result;

      if (count == -1)
         count = m_stringLength - startIndex;
      else if (startIndex+count >= m_stringLength)
         count = m_stringLength - startIndex;

      int i = 0, j = 0;
      char *holder = new char[m_stringLength+1];

      for (i = startIndex; i < startIndex + count; i++)
         holder[j++] = m_bufferPtr[i];

      holder[j] = 0;
      result.Assign (holder);

      delete [] holder;
      return result;
   }

   //
   // Function: Mid
   //  Gets the substring by specified bounds.
   //
   // Parameters:
   //  startIndex - Start index to get from.
   //
   // Returns:
   //  Tokenized string.
   //
   String Mid (int startIndex)
   {
      return Mid (startIndex, m_stringLength - startIndex);
   }

   //
   // Function: Left
   //  Gets the string from left side.
   //
   // Parameters:
   //  count - Number of characters to get.
   //
   // Returns:
   //  Tokenized string.
   //
   String Left (int count)
   {
      return Mid (0, count);
   }

   //
   // Function: Right
   //  Gets the string from right side.
   //
   // Parameters:
   //  count - Number of characters to get.
   //
   // Returns:
   //  Tokenized string.
   //
   String Right (int count)
   {
      if (count > m_stringLength)
         count = m_stringLength;

      return Mid (m_stringLength - count, count);
   }

   //
   // Function: ToUpper
   //  Gets the string in upper case.
   //
   // Returns:
   //  Upped sting.
   //
   String ToUpper (void)
   {
      String result;

      for (int i = 0; i < GetLength (); i++)
         result += static_cast <char> (toupper (static_cast <int> (m_bufferPtr[i])));

      return result;
   }

   //
   // Function: ToUpper
   //  Gets the string in upper case.
   //
   // Returns:
   //  Lowered sting.
   //
   String ToLower (void)
   {
      String result;

      for (int i = 0; i < GetLength (); i++)
         result += static_cast <char> (tolower (static_cast <int> (m_bufferPtr[i])));

      return result;
   }

   //
   // Function: ToReverse
   //  Reverses the string.
   //
   // Returns:
   //  Reversed string.
   //
   String ToReverse (void)
   {
      char *source = m_bufferPtr + GetLength () - 1;
      char *dest = m_bufferPtr;

      while (source > dest)
      {
         if (*source == *dest)
         {
            source--;
            dest++;
         }
         else
         {
            char ch = *source;

            *source-- = *dest;
            *dest++ = ch;
         }
      }
      return m_bufferPtr;
   }

   //
   // Function: MakeUpper
   //  Converts string to upper case.
   //
   void MakeUpper (void)
   {
      *this = ToUpper ();
   }

   //
   // Function: MakeLower
   //  Converts string to lower case.
   //
   void MakeLower (void)
   {
      *this = ToLower ();
   }

   //
   // Function: MakeReverse
   //  Converts string into reverse order.
   //
   void MakeReverse (void)
   {
      *this = ToReverse ();
   }

   //
   // Function: Compare
   //  Compares string with other string.
   //
   // Parameters:
   //  string - String t compare with.
   //
   // Returns:
   //  Zero if they are equal.
   //
   int Compare (const String &string) const
   {
      return strcmp (m_bufferPtr, string.m_bufferPtr);
   }

   //
   // Function: Compare
   //  Compares string with other string.
   //
   // Parameters:
   //  str - String t compare with.
   //
   // Returns:
   //  Zero if they are equal.
   //
   int Compare (const char *str) const
   {
      return strcmp (m_bufferPtr, str);
   }

   //
   // Function: Collate
   //  Collate the string.
   //
   // Parameters:
   //  string - String to collate.
   //
   // Returns:
   //  One on success.
   //
   int Collate (const String &string) const
   {
      return strcoll (m_bufferPtr, string.m_bufferPtr);
   }

   //
   // Function: Find
   //  Find the character.
   //
   // Parameters:
   //  input - Character to search for.
   //
   // Returns:
   //  Index of character.
   //
   int Find (char input) const
   {
      return Find (input, 0);
   }

   //
   // Function: Find
   //  Find the character.
   //
   // Parameters:
   //  input - Character to search for.
   //  startIndex - Start index to search from.
   //
   // Returns:
   //  Index of character.
   //
   int Find (char input, int startIndex) const
   {
      char *str = m_bufferPtr + startIndex;

      for (;;)
      {
         if (*str == input)
            return str - m_bufferPtr;

         if (*str == 0)
            return -1;

         str++;
      }
   }

   //
   // Function: Find
   //  Tries to find string.
   //
   // Parameters:
   //  string - String to search for.
   //
   // Returns:
   //  Position of found string.
   //
   int Find (const String &string) const
   {
      return Find (string, 0);
   }

   //
   // Function: Find
   //  Tries to find string from specified index.
   //
   // Parameters:
   //  string - String to search for.
   //  startIndex - Index to start search from.
   //
   // Returns:
   //  Position of found string.
   //
   int Find (const String &string, int startIndex) const
   {
      if (string.m_stringLength == 0)
         return startIndex;

      for (; startIndex < m_stringLength; startIndex++)
      {
         int j;

         for (j = 0; j < string.m_stringLength && startIndex + j < m_stringLength; j++)
         {
            if (m_bufferPtr[startIndex + j] != string.m_bufferPtr[j])
               break;
         }

         if (j == string.m_stringLength)
            return startIndex;
      }
      return -1;
   }

   //
   // Function: ReverseFind
   //  Tries to find character in reverse order.
   //
   // Parameters:
   //  ch - Character to search for.
   //
   // Returns:
   //  Position of found character.
   //
   int ReverseFind (char ch)
   {
      if (m_stringLength == 0)
         return -1;

      char *str = m_bufferPtr + m_stringLength - 1;

      for (;;)
      {
         if (*str == ch)
            return str - m_bufferPtr;

         if (str == m_bufferPtr)
            return -1;
         str--;
      }
   }

   //
   // Function: FindOneOf
   //  Find one of occurrences of string.
   //
   // Parameters:
   //  string - String to search for.
   //
   // Returns:
   //  -1 in case of nothing is found, start of string in buffer otherwise.
   //
   int FindOneOf (const String &string)
   {
      for (int i = 0; i < m_stringLength; i++)
      {
         if (string.Find (m_bufferPtr[i]) >= 0)
            return i;
      }
      return -1;
   }

   //
   // Function: TrimRight
   //  Trims string from right side.
   //
   // Returns:
   //  Trimmed string.
   //
   String &TrimRight (void)
   {
      char *str = m_bufferPtr;
      char *last = NULL;

      while (*str != 0)
      {
         if (IsTrimChar (*str))
         {
            if (last == NULL)
               last = str;
         }
         else
            last = NULL;
         str++;
      }

      if (last != NULL)
         Delete (last - m_bufferPtr);

      return *this;
   }

   //
   // Function: TrimLeft
   //  Trims string from left side.
   //
   // Returns:
   //  Trimmed string.
   //
   String &TrimLeft (void)
   {
      char *str = m_bufferPtr;

      while (IsTrimChar (*str))
         str++;

      if (str != m_bufferPtr)
      {
         int first = int (str - GetBuffer ());
         char *buffer = GetBuffer (GetLength ());

         str = buffer + first;
         int length = GetLength () - first;

         memmove (buffer, str, (length + 1) * sizeof (char));
         ReleaseBuffer (length);
      }
      return *this;
   }

   //
   // Function: Trim
   //  Trims string from both sides.
   //
   // Returns:
   //  Trimmed string.
   //
   String &Trim (void)
   {
      return TrimRight ().TrimLeft ();
   }

   //
   // Function: TrimRight
   //  Trims specified character at the right of the string.
   //
   // Parameters:
   //  ch - Character to trim.
   //
   void TrimRight (char ch)
   {
      const char *str = m_bufferPtr;
      const char *last = NULL;

      while (*str != 0)
      {
         if (*str == ch)
         {
            if (last == NULL)
               last = str;
         }
         else
            last = NULL;

         str++;
      }
      if (last != NULL)
      {
         int i = last - m_bufferPtr;
         Delete (i, m_stringLength - i);
      }
   }

   //
   // Function: TrimLeft
   //  Trims specified character at the left of the string.
   //
   // Parameters:
   //  ch - Character to trim.
   //
   void TrimLeft (char ch)
   {
      char *str = m_bufferPtr;

      while (ch == *str)
         str++;

      Delete (0, str - m_bufferPtr);
   }

   //
   // Function: Insert
   //  Inserts character at specified index.
   //
   // Parameters:
   //  index - Position to insert string.
   //  ch - Character to insert.
   //
   // Returns:
   //  New string length.
   //
   int Insert (int index, char ch)
   {
      InsertSpace (index, 1);

      m_bufferPtr[index] = ch;
      m_stringLength++;

      return m_stringLength;
   }

   //
   // Function: Insert
   //  Inserts string at specified index.
   //
   // Parameters:
   //  index - Position to insert string.
   //  string - Text to insert.
   //
   // Returns:
   //  New string length.
   //
   int Insert (int index, const String &string)
   {
      CorrectIndex (index);

      if (string.m_stringLength == 0)
         return m_stringLength;

      int numInsertChars = string.m_stringLength;
      InsertSpace (index, numInsertChars);

      for(int i = 0; i < numInsertChars; i++)
         m_bufferPtr[index + i] = string[i];
      m_stringLength += numInsertChars;

      return m_stringLength;
   }

   //
   // Function: Replace
   //  Replaces old characters with new one.
   //
   // Parameters:
   //  oldCharacter - Old character to replace.
   //  newCharacter - New character to replace with.
   //
   // Returns:
   //  Number of occurrences replaced.
   //
   int Replace (char oldCharacter, char newCharacter)
   {
      if (oldCharacter == newCharacter)
         return 0;

      static int num = 0;
      int position = 0;

      while (position < GetLength ())
      {
         position = Find (oldCharacter, position);

         if (position < 0)
            break;

         m_bufferPtr[position] = newCharacter;

         position++;
         num++;
      }
      return num;
   }

   //
   // Function: Replace
   //  Replaces string in other string.
   //
   // Parameters:
   //  oldString - Old string to replace.
   //  newString - New string to replace with.
   //
   // Returns:
   //  Number of characters replaced.
   //
   int Replace (const String &oldString, const String &newString)
   {
      if (oldString.m_stringLength == 0)
         return 0;

      if (newString.m_stringLength == 0)
         return 0;

      int oldLength = oldString.m_stringLength;
      int newLength = newString.m_stringLength;

      int num = 0;
      int position = 0;

      while (position < m_stringLength)
      {
         position = Find (oldString, position);

         if (position < 0)
            break;

         Delete (position, oldLength);
         Insert (position, newString);

         position += newLength;
         num++;
      }
      return num;
   }

   //
   // Function: Delete
   //  Deletes characters from string.
   //
   // Parameters:
   //  index - Start of characters remove.
   //  count - Number of characters to remove.
   //
   // Returns:
   //  New string length.
   //
   int Delete (int index, int count = 1)
   {
      if (index + count > m_stringLength)
         count = m_stringLength - index;

      if (count > 0)
      {
         MoveItems (index, index + count);
         m_stringLength -= count;
      }
      return m_stringLength;
   }

   //
   // Function: TrimQuotes
   //  Trims trailing quotes.
   //
   // Returns:
   //  Trimmed string.
   //
   String TrimQuotes (void)
   {
      TrimRight ('\"');
      TrimRight ('\'');

      TrimLeft ('\"');
      TrimLeft ('\'');

      return *this;
   }

   //
   // Function: Contains
   //  Checks whether string contains something.
   //
   // Parameters:
   //  what - String to check.
   //
   // Returns:
   //  True if string exists, false otherwise.
   //
   bool Contains (const String &what)
   {
      return strstr (m_bufferPtr, what.m_bufferPtr) != NULL;
   }

   //
   // Function: Hash
   //  Gets the string hash.
   //
   // Returns:
   //  Hash of the string.
   //
   unsigned long Hash (void)
   {
      unsigned long hash = 0;
      const char *ptr = m_bufferPtr;

      while (*ptr)
      {
         hash = (hash << 5) + hash + (*ptr);
         ptr++;
      }
      return hash;
   }

   //
   // Function: Split
   //  Splits string using string separator.
   //
   // Parameters:
   //  separator - Separator to split with.
   //
   // Returns:
   //  Array of slitted strings.
   //
   // See Also:
   //  <Array>
   //
   Array <String> Split (const char *separator)
   {
      Array <String> holder;
      int tokenLength, index = 0;

      do
      {
         index += strspn (&m_bufferPtr[index], separator);
         tokenLength = strcspn (&m_bufferPtr[index], separator);

         if (tokenLength > 0)
            holder.Push (Mid (index, tokenLength));

         index += tokenLength;

      } while (tokenLength > 0);

      return holder;
   }

   //
   // Function: Split
   //  Splits string using character.
   //
   // Parameters:
   //  separator - Separator to split with.
   //
   // Returns:
   //  Array of slitted strings.
   //
   // See Also:
   //  <Array>
   //
   Array <String> Split (char separator)
   {
      char sep[2];

      sep[0] = separator;
      sep[1] = 0x0;

      return Split (sep);
   }

public:
   //
   // Function: TrimExternalBuffer
   //  Trims string from both sides.
   //
   // Returns:
   //  None
   //
   static inline void TrimExternalBuffer (char *str)
   {
      int pos = 0;
      char *dest = str;

      while (str[pos] <= ' ' && str[pos] > 0)
         pos++;

      while (str[pos])
      {
         *(dest++) = str[pos];
         pos++;
      }
      *(dest--) = '\0';

      while (dest >= str && *dest <= ' ' && *dest > 0)
         *(dest--) = '\0';
   }
};

//
// Class: File
//  Simple STDIO file wrapper class.
//
class File
{
protected:
   FILE *m_handle;
   int m_fileSize;

//
// Group: (Con/De)structors
//
public:

   //
   // Function: File
   //  Default file class, constructor.
   //
   File (void)
   {
      m_handle = NULL;
      m_fileSize = 0;
   }

   //
   // Function: File
   //  Default file class, constructor, with file opening.
   //
   File (String fileName, String mode = "rt")
   {
      Open (fileName, mode);
   }

   //
   // Function: ~File
   //  Default file class, destructor.
   //
   ~File (void)
   {
      Close ();
   }

   //
   // Function: Open
   //  Opens file and gets it's size.
   //
   // Parameters:
   //  fileName - String containing file name.
   //  mode - String containing open mode for file.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool Open (const String &fileName, const String &mode)
   {
      if ((m_handle = fopen (fileName.GetBuffer (), mode.GetBuffer ())) == NULL)
         return false;

      fseek (m_handle, 0L, SEEK_END);
      m_fileSize = ftell (m_handle);
      fseek (m_handle, 0L, SEEK_SET);

      return true;
   }

   //
   // Function: Close
   //  Closes file, and destroys STDIO file object.
   //
   void Close (void)
   {
      if (m_handle != NULL)
      {
         fclose (m_handle);
         m_handle = NULL;
      }
      m_fileSize = 0;
   }

   //
   // Function: Eof
   //  Checks whether we reached end of file.
   //
   // Returns:
   //  True if reached, false otherwise.
   //
   bool Eof (void)
   {
      assert (m_handle != NULL);
      return feof (m_handle) ? true : false;
   }

   //
   // Function: Flush
   //  Flushes file stream.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool Flush (void)
   {
      assert (m_handle != NULL);
      return fflush (m_handle) ? false : true;
   }

   //
   // Function: GetChar
   //  Pops one character from the file stream.
   //
   // Returns:
   //  Popped from stream character
   //
   int GetChar (void)
   {
      assert (m_handle != NULL);
      return fgetc (m_handle);
   }

   //
   // Function: GetBuffer
   //  Gets the single line, from the non-binary stream.
   //
   // Parameters:
   //  buffer - Pointer to buffer, that should receive string.
   //  count - Max. size of buffer.
   //
   // Returns:
   //  Pointer to string containing popped line.
   //
   char *GetBuffer (char *buffer, int count)
   {
      assert (m_handle != NULL);
      return fgets (buffer, count, m_handle);
   }

   //
   // Function: Printf
   //  Puts formatted buffer, into stream.
   //
   // Parameters:
   //  format - 
   //
   // Returns:
   //  Number of bytes, that was written.
   //
   int Printf (const char *format, ...)
   {
      assert (m_handle != NULL);

      va_list ap;
      va_start (ap, format);
      int written = vfprintf (m_handle, format, ap);
      va_end (ap);

      if (written < 0)
         return 0;

      return written;
   }

   //
   // Function: PutChar
   //  Puts character into file stream.
   //
   // Parameters:
   //  ch - Character that should be put into stream.
   //
   // Returns:
   //  Character that was putted into the stream.
   //
   char PutChar (char ch)
   {
      assert (m_handle != NULL);
      return static_cast <char> (fputc (ch, m_handle));
   }

   //
   // Function: PutString
   //  Puts buffer into the file stream.
   //
   // Parameters:
   //  buffer - Buffer that should be put, into stream.
   //
   // Returns:
   //  True, if operation succeeded, false otherwise.
   //
   bool PutString (String buffer)
   {
      assert (m_handle != NULL);

      if (fputs (buffer.GetBuffer (), m_handle) < 0)
         return false;

      return true;
   }

   //
   // Function: Read
   //  Reads buffer from file stream in binary format.
   //
   // Parameters:
   //  buffer - Holder for read buffer.
   //  size - Size of the buffer to read.
   //  count - Number of buffer chunks to read.
   //
   // Returns:
   //  Number of bytes red from file.
   //
   int Read (void *buffer, int size, int count = 1)
   {
      assert (m_handle != NULL);
      return fread (buffer, size, count, m_handle);
   }

   //
   // Function: Write
   //  Writes binary buffer into file stream.
   //
   // Parameters:
   //  buffer - Buffer holder, that should be written into file stream.
   //  size - Size of the buffer that should be written.
   //  count - Number of buffer chunks to write.
   //
   // Returns:
   //  Numbers of bytes written to file.
   //
   int Write (void *buffer, int size, int count = 1)
   {
      assert (m_handle != NULL);
      return fwrite (buffer, size, count, m_handle);
   }

   //
   // Function: Seek
   //  Seeks file stream with specified parameters.
   //
   // Parameters:
   //  offset - Offset where cursor should be set.
   //  origin - Type of offset set.
   //
   // Returns:
   //  True if operation success, false otherwise.
   //
   bool Seek (long offset, int origin)
   {
      assert (m_handle != NULL);

      if (fseek (m_handle, offset, origin) != 0)
         return false;

      return true;
   }

   //
   // Function: Rewind
   //  Rewinds the file stream.
   //
   void Rewind (void)
   {
      assert (m_handle != NULL);
      rewind (m_handle);
   }

   //
   // Function: GetSize
   //  Gets the file size of opened file stream.
   //
   // Returns:
   //  Number of bytes in file.
   //
   int GetSize (void)
   {
      return m_fileSize;
   }

   //
   // Function: IsValid
   //  Checks whether file stream is valid.
   //
   // Returns:
   //  True if file stream valid, false otherwise.
   //
   bool IsValid (void)
   {
      return m_handle != NULL;
   }

public:
   static inline bool Accessible (const String &filename)
   {
      File fp;

      if (fp.Open (filename, "rb"))
      {
         fp.Close ();
         return true;
      }
      return false;
   }

   static inline void CreatePath (char *path)
   {
      for (char *ofs = path + 1; *ofs; ofs++)
      {
         if (*ofs == '/')
         {
            // create the directory
            *ofs = 0;
            _mkdir (path);
            *ofs = '/';
         }
      }
      _mkdir (path);
   }
};

//
// Class: MemoryFile
//  Simple Memory file wrapper class. (RO)
//
class MemoryFile
{
public:
   typedef uint8 *(*MF_Loader) (const char *, int *);
   typedef void (*MF_Unloader) (uint8 *);

public:
   static MF_Loader Loader;
   static MF_Unloader Unloader;

protected:
   int m_size;
   int m_pos;
   uint8 *m_buffer;

   //
   // Group: (Con/De)structors
   //
public:

   //
   // Function: File
   //  Default file class, constructor.
   //
   MemoryFile (void)
   {
      m_size = 0;
      m_pos = 0;

      m_buffer = NULL;
   }

   //
   // Function: File
   //  Default file class, constructor, with file opening.
   //
   MemoryFile (const String &fileName)
   {
      m_size = 0;
      m_pos = 0;

      m_buffer = NULL;

      Open (fileName);
   }

   //
   // Function: ~File
   //  Default file class, destructor.
   //
   ~MemoryFile (void)
   {
      Close ();
   }

   //
   // Function: Open
   //  Opens file and gets it's size.
   //
   // Parameters:
   //  fileName - String containing file name.
   //
   // Returns:
   //  True if operation succeeded, false otherwise.
   //
   bool Open (const char *fileName)
   {
      if (!Loader)
         return false;

      m_size = 0;
      m_pos = 0;

      m_buffer = Loader (fileName, &m_size);

      if (m_buffer == NULL || m_size < 0)
         return false;

      return true;
   }

   //
   // Function: Close
   //  Closes file, and destroys STDIO file object.
   //
   void Close (void)
   {
      if (Unloader != NULL)
         Unloader (m_buffer);

      m_size = 0;
      m_pos = 0;

      m_buffer = NULL;
   }

   //
   // Function: GetChar
   //  Pops one character from the file stream.
   //
   // Returns:
   //  Popped from stream character
   //
   int GetChar (void)
   {
      if (m_buffer == NULL || m_pos >= m_size)
         return -1;

      int readCh = m_buffer[m_pos];
      m_pos++;

      return readCh;
   }

   //
   // Function: GetBuffer
   //  Gets the single line, from the non-binary stream.
   //
   // Parameters:
   //  buffer - Pointer to buffer, that should receive string.
   //  count - Max. size of buffer.
   //
   // Returns:
   //  Pointer to string containing popped line.
   //
   char *GetBuffer (char *buffer, int count)
   {
      if (m_buffer == NULL || m_pos >= m_size)
         return NULL;

      int start = m_pos;
      int end = m_size - 1;
      
      if (m_size - m_pos > count - 1)
         end = m_pos + count - 1;

      while (m_pos < end)
      {
         if (m_buffer[m_pos] == 0x0a)
            end = m_pos;

         m_pos++;
      }

      if (m_pos == start)
         return NULL;

      int pos = start;

      for (; pos <= end; pos++)
         buffer[pos - start] = m_buffer[pos];

      if (buffer[pos - start - 2] == 0x0d)
      {
         buffer[pos - start - 2] = '\n';
         pos--;
      }

      if (buffer[pos - start - 1] == 0x0d || buffer[pos - start - 1] == 0x0a)
         buffer[pos - start - 1] = '\n';

      buffer[pos - start] = 0;

      return buffer;
   }

   //
   // Function: Read
   //  Reads buffer from file stream in binary format.
   //
   // Parameters:
   //  buffer - Holder for read buffer.
   //  size - Size of the buffer to read.
   //  count - Number of buffer chunks to read.
   //
   // Returns:
   //  Number of bytes red from file.
   //
   int Read (void *buffer, int size, int count = 1)
   {
      if (!m_buffer|| m_pos >= m_size || buffer == NULL || !size || !count)
         return 0;

      int blocksRead = A_min ((m_size - m_pos) / size, count) * size;

      memcpy (buffer, &m_buffer[m_pos], blocksRead);
      m_pos += blocksRead;

      return blocksRead;
   }

   //
   // Function: Seek
   //  Seeks file stream with specified parameters.
   //
   // Parameters:
   //  offset - Offset where cursor should be set.
   //  origin - Type of offset set.
   //
   // Returns:
   //  True if operation success, false otherwise.
   //
   bool Seek (long offset, int origin)
   {
      if (m_buffer == NULL || m_pos >= m_size)
         return false;

      if (origin == SEEK_SET)
      {
         if (offset >= m_size)
            return false;

         m_pos = offset;
      }
      else if (origin == SEEK_END)
      {
         if (offset >= m_size)
            return false;

         m_pos = m_size - offset;
      }
      else
      {
         if (m_pos + offset >= m_size)
            return false;

         m_pos += offset;
      }
      return true;
   }

   //
   // Function: GetSize
   //  Gets the file size of opened file stream.
   //
   // Returns:
   //  Number of bytes in file.
   //
   int GetSize (void)
   {
      return m_size;
   }

   //
   // Function: IsValid
   //  Checks whether file stream is valid.
   //
   // Returns:
   //  True if file stream valid, false otherwise.
   //
   bool IsValid (void)
   {
      return m_buffer != NULL && m_size > 0;
   }
};

//
// Type: StrVec
//  Array of strings.
//
typedef Array <const String &> StrVec;

#ifndef FORCEINLINE
#define FORCEINLINE inline
#endif

//
// Class: Singleton
//  Implements no-copying singleton.
//
template <typename T> class Singleton
{
//
// Group: (Con/De)structors
//
protected:

   //
   // Function: Singleton
   //  Default singleton constructor.
   //
   Singleton (void) { }

   //
   // Function: ~Singleton
   //  Default singleton destructor.
   //
   virtual ~Singleton (void) { }

private:
   Singleton (Singleton const &);
   void operator = (Singleton const &);

public:

   //
   // Function: GetObject
   //  Gets the object of singleton.
   //
   // Returns:
   //  Object pointer.
   //  
   //
   static FORCEINLINE T *GetObject (void)
   {
      return &GetReference ();
   }

   //
   // Function: GetObject
   //  Gets the object of singleton as reference.
   //
   // Returns:
   //  Object reference.
   //  
   //
   static FORCEINLINE T &GetReference (void)
   {
      static T reference;
      return reference;
   };
};

//
// Macro: ThrowException
//  Throws debug exception.
//
// Parameters:
//  text - Text of error.
//
#define ThrowException(text) \
   throw Exception (text, __FILE__, __LINE__)


//
// Macro: IterateArray
//  Utility macro for iterating arrays.
//
// Parameters:
//  iteratorName - Name of the iterator.
//  arrayName - Name of the array that should be iterated.
//
// See Also:
//  <Array>
//
#define FOR_EACH_AE(arrayName, iteratorName) \
   for (int iteratorName = 0; iteratorName != arrayName.GetElementNumber (); iteratorName++)


//
// Sizeof bounds
//
#define SIZEOF_CHAR(in) sizeof (in) - 1

//
// Squared Length
//
#define GET_SQUARE(in) (in * in)
