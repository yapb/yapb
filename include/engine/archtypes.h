/***
*
*   Copyright (c) 1999-2005, Valve Corporation. All rights reserved.
*
*   This product contains software technology licensed from Id 
*   Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*   All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
#ifndef ARCHTYPES_H
#define ARCHTYPES_H

#ifdef _WIN32
#pragma once
#endif

#include <string.h>

// detects the build platform
#if defined (__linux__) || defined (__debian__) || defined (__linux)
#define __linux__ 1
   
#endif

// for when we care about how many bits we use
typedef signed char int8;
typedef signed short int16;
typedef signed long int32;

#ifdef _WIN32
#ifdef _MSC_VER
typedef signed __int64 int64;
#endif
#elif defined __linux__
typedef long long int64;
#endif

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;

#ifdef _WIN32
#ifdef _MSC_VER
typedef unsigned __int64 uint64;
#endif
#elif defined __linux__
typedef unsigned long long uint64;
#endif

typedef float float32;
typedef double float64;

// for when we don't care about how many bits we use
typedef unsigned int uint;

// This can be used to ensure the size of pointers to members when declaring
// a pointer type for a class that has only been forward declared
#ifdef _MSC_VER
#define SINGLE_INHERITANCE __single_inheritance
#define MULTIPLE_INHERITANCE __multiple_inheritance
#else
#define SINGLE_INHERITANCE
#define MULTIPLE_INHERITANCE
#endif

// need these for the limits
#include <limits.h>
#include <float.h>

// Maximum and minimum representable values
#define  INT8_MAX    SCHAR_MAX
#define  INT16_MAX   SHRT_MAX
#define  INT32_MAX   LONG_MAX
#define  INT64_MAX  (((int64)~0) >> 1)

#define  INT8_MIN    SCHAR_MIN
#define  INT16_MIN   SHRT_MIN
#define  INT32_MIN   LONG_MIN
#define  INT64_MIN  (((int64)1) << 63)

#define  UINT8_MAX  ((uint8)~0)
#define  UINT16_MAX ((uint16)~0)
#define  UINT32_MAX ((uint32)~0)
#define  UINT64_MAX ((uint64)~0)

#define  UINT8_MIN   0
#define  UINT16_MIN  0
#define  UINT32_MIN  0
#define  UINT64_MIN  0

#ifndef  UINT_MIN
#define  UINT_MIN    UINT32_MIN
#endif

#define  FLOAT32_MAX FLT_MAX
#define  FLOAT64_MAX DBL_MAX

#define  FLOAT32_MIN FLT_MIN
#define  FLOAT64_MIN DBL_MIN

// portability / compiler settings
#if defined(_WIN32) && !defined(WINDED)

#if defined(_M_IX86)
#define __i386__   1
#endif

#elif __linux__
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef void *HINSTANCE;

#define _MAX_PATH PATH_MAX
#endif // defined(_WIN32) && !defined(WINDED)

// Defines MAX_PATH
#ifndef MAX_PATH
#define MAX_PATH  260
#endif

// Used to step into the debugger
#define  DebuggerBreak()  __asm { int 3 }

// C functions for external declarations that call the appropriate C++ methods
#ifndef EXPORT
#ifdef _WIN32
#define EXPORT   __declspec( dllexport )
#else
#define EXPORT                    /* */
#endif
#endif

#if defined __i386__ && !defined __linux__
#define id386   1
#else
#define id386   0
#endif // __i386__

#ifdef _WIN32
// Used for dll exporting and importing
#define  DLL_EXPORT   extern "C" __declspec( dllexport )
#define  DLL_IMPORT   extern "C" __declspec( dllimport )

// Can't use extern "C" when DLL exporting a class
#define  DLL_CLASS_EXPORT   __declspec( dllexport )
#define  DLL_CLASS_IMPORT   __declspec( dllimport )

// Can't use extern "C" when DLL exporting a global
#define  DLL_GLOBAL_EXPORT   extern __declspec( dllexport )
#define  DLL_GLOBAL_IMPORT   extern __declspec( dllimport )
#elif defined __linux__ || defined (__APPLE__)

// Used for dll exporting and importing
#define  DLL_EXPORT   extern "C"
#define  DLL_IMPORT   extern "C"

// Can't use extern "C" when DLL exporting a class
#define  DLL_CLASS_EXPORT
#define  DLL_CLASS_IMPORT

// Can't use extern "C" when DLL exporting a global
#define  DLL_GLOBAL_EXPORT   extern
#define  DLL_GLOBAL_IMPORT   extern

#else
#error "Unsupported Platform."
#endif

#ifndef _WIN32
#define FAKEFUNC (void *) 0
#else
#define FAKEFUNC __noop
#endif

// Used for standard calling conventions
#ifdef _WIN32
#define  STDCALL            __stdcall
#define  FASTCALL            __fastcall
#ifndef FORCEINLINE
#define  FORCEINLINE         __forceinline
#endif
#else
#define  STDCALL
#define  FASTCALL
#define  FORCEINLINE         inline
#endif

// Force a function call site -not- to inlined. (useful for profiling)
#define DONT_INLINE(a) (((int)(a)+1)?(a):(a))

// Pass hints to the compiler to prevent it from generating unnessecary / stupid code
// in certain situations.  Several compilers other than MSVC also have an equivilent 
// construct.
//
// Essentially the 'Hint' is that the condition specified is assumed to be true at 
// that point in the compilation.  If '0' is passed, then the compiler assumes that
// any subsequent code in the same 'basic block' is unreachable, and thus usually 
// removed.
#ifdef _MSC_VER
#define HINT(THE_HINT)   __assume((THE_HINT))
#else
#define HINT(THE_HINT)   0
#endif

// Marks the codepath from here until the next branch entry point as unreachable,
// and asserts if any attempt is made to execute it.
#define UNREACHABLE() { ASSERT(0); HINT(0); }

// In cases where no default is present or appropriate, this causes MSVC to generate 
// as little code as possible, and throw an assertion in debug.
#define NO_DEFAULT default: UNREACHABLE();

#ifdef _WIN32
// Alloca defined for this platform
#define  stackalloc( _size ) _alloca( _size )
#define  stackfree( _p )   0
#elif __linux__
// Alloca defined for this platform
#define  stackalloc( _size ) alloca( _size )
#define  stackfree( _p )   0
#endif

#endif
