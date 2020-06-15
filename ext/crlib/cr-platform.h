//
// CRLib - Simple library for STL replacement in private projects.
// Copyright © 2020 YaPB Development Team <team@yapb.ru>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 

#pragma once

CR_NAMESPACE_BEGIN

// detects the build platform
#if defined(__linux__)
#  define CR_LINUX
#elif defined(__APPLE__)
#  define CR_OSX
#elif defined(_WIN32)
#  define CR_WINDOWS
#endif

#if defined(__ANDROID__)
#  define CR_ANDROID
#     if defined(LOAD_HARDFP)
#        define CR_ANDROID_HARD_FP
#     endif
#endif

// detects the compiler
#if defined(_MSC_VER)
#  define CR_CXX_MSVC _MSC_VER
#endif

#if defined(__clang__)
#  define CR_CXX_CLANG __clang__
#endif

#if defined(__INTEL_COMPILER)
#  define CR_CXX_INTEL __INTEL_COMPILER
#endif

#if defined(__GNUC__)
#  define CR_CXX_GCC __GNUC__
#endif

// configure macroses
#define CR_LINKAGE_C extern "C"

#if defined(CR_WINDOWS)
#  define CR_EXPORT CR_LINKAGE_C __declspec (dllexport)
#  define CR_STDCALL __stdcall
#elif defined(CR_LINUX) || defined(CR_OSX)
#  define CR_EXPORT CR_LINKAGE_C __attribute__((visibility("default")))
#  define CR_STDCALL
#else
#  error "Can't configure export macros. Compiler unrecognized."
#endif

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) || defined(__amd64) || (defined(_MSC_VER) && defined(_M_X64))
#  define CR_ARCH_X64
#elif defined(__i686) || defined(__i686__) || defined(__i386) || defined(__i386__) || defined(i386) || (defined(_MSC_VER) && defined(_M_IX86))
#  define CR_ARCH_X86
#endif

#if defined(__arm__)
#  define CR_ARCH_ARM
#  if defined(__aarch64__)
#     define CR_ARCH_ARM64
#  endif
#endif 

#if (defined(CR_ARCH_X86) || defined(CR_ARCH_X64)) && !defined(CR_DEBUG)
#  define CR_HAS_SSE
#endif

#if defined(CR_HAS_SSE)
#  if defined (CR_CXX_MSVC)
#     define CR_ALIGN16 __declspec (align (16))
#  else
#     define CR_ALIGN16 __attribute__((aligned(16)))
#  endif
#endif

// disable warnings regarding intel compiler
#if defined(CR_CXX_INTEL)
#  pragma warning (disable : 3280) // declaration hides member "XXX" (declared at line XX)
#  pragma warning (disable : 2415) // variable "XXX" of static storage duration was declared but never referenced
#  pragma warning (disable : 873)  // function "operator new(size_t={unsigned int}, void *)" has no corresponding operator delete (to be called if an exception is thrown during initialization of an allocated object)
#  pragma warning (disable : 383)  // value copied to temporary, reference to temporary used
#  pragma warning (disable : 11074 11075) // remarks about inlining bla-bla-bla
#endif

// msvc provides us placement new by default
#if defined (CR_CXX_MSVC)
#  define __PLACEMENT_NEW_INLINE 1
#endif

CR_NAMESPACE_END

#if defined(CR_WINDOWS)
#  define WIN32_LEAN_AND_MEAN 
#  include <windows.h>
#  include <direct.h>

#  if defined (max) 
#     undef max
#  endif

#  if defined (min) 
#     undef min
#  endif

#else
#  include <unistd.h>
#  include <strings.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#endif

#include <stdio.h>
#include <assert.h>
#include <locale.h>
#include <string.h>
#include <stdarg.h>

#if defined (CR_ANDROID)
#  include <android/log.h>
#endif

CR_NAMESPACE_BEGIN

// helper struct for platform detection
struct Platform : public Singleton <Platform> {
   bool win32 = false;
   bool linux = false;
   bool osx = false;
   bool android = false;
   bool hfp = false;
   bool x64 = false;
   bool arm = false;

   Platform () {
#if defined(CR_WINDOWS)
      win32 = true;
#endif

#if defined(CR_ANDROID)
      android = true;

#  if defined (CR_ANDROID_HARD_FP)
         hfp = true;
#  endif
#endif

#if defined(CR_LINUX)
      linux = true;
#endif

#if defined(CR_OSX)
      osx = true;
#endif

#if defined(CR_ARCH_X64) || defined(CR_ARCH_ARM64)
      x64 = true;
#endif

#if defined(CR_ARCH_ARM)
      arm = true;
      android = true;
#endif
   }

   // helper platform-dependant functions
   template <typename U> bool checkPointer (U *ptr) {
#if defined(CR_WINDOWS)
      if (IsBadCodePtr (reinterpret_cast <FARPROC> (ptr))) {
         return false;
      }
#else
      (void) (ptr);
#endif
      return true;
   }

   bool createDirectory (const char *dir) {
      int result = 0;
#if defined(CR_WINDOWS)
      result = _mkdir (dir);
#else
      result = mkdir (dir, 0777);
#endif
      return !!result;
   }

   bool removeFile (const char *dir) {
#if defined(CR_WINDOWS)
      _unlink (dir);
#else
      unlink (dir);
#endif
      return true;
   }

   bool hasModule (const char *mod) {
#if defined(CR_WINDOWS)
      return GetModuleHandleA (mod) != nullptr;
#else
      (void) (mod);
      return true;
#endif
   }

   float seconds () {
#if defined(CR_WINDOWS)
      LARGE_INTEGER count, freq;

      count.QuadPart = 0;
      freq.QuadPart = 0;

      QueryPerformanceFrequency (&freq);
      QueryPerformanceCounter (&count);

      return static_cast <float> (count.QuadPart) / static_cast <float> (freq.QuadPart);
#else
      timeval tv;
      gettimeofday (&tv, NULL);

      return static_cast <float> (tv.tv_sec) + (static_cast <float> (tv.tv_usec)) / 1000000.0;
#endif
   }

   void abort (const char *msg = "OUT OF MEMORY!") noexcept {
      fprintf (stderr, "%s\n", msg);

#if defined (CR_ANDROID)
      __android_log_write (ANDROID_LOG_ERROR, "crlib.fatal", msg);
#endif

#if defined(CR_WINDOWS)
      DestroyWindow (GetForegroundWindow ());
      MessageBoxA (GetActiveWindow (), msg, "crlib.fatal", MB_ICONSTOP);
#endif
      ::abort ();
   }

   // anologue of memset
   template <typename U> void bzero (U *ptr, size_t len) noexcept {
#if defined (CR_WINDOWS)
      memset (ptr, 0, len);
#else
      auto zeroing = reinterpret_cast <uint8 *> (ptr);

      for (size_t i = 0; i < len; ++i) {
         zeroing[i] = 0;
      }
#endif
   }

   const char *env (const char *var) {
      static char result[256];
      bzero (result, sizeof (result));

#if defined(CR_WINDOWS) && !defined(CR_CXX_GCC)
      char *buffer = nullptr;
      size_t size = 0;

      if (_dupenv_s (&buffer, &size, var) == 0 && buffer != nullptr) {
         strncpy_s (result, buffer, sizeof (result));
         free (buffer);
      }
#else
      strncpy (result, getenv (var), cr::bufsize (result));
#endif
      return result;
   }
};

// expose platform singleton
CR_EXPOSE_GLOBAL_SINGLETON (Platform, plat);

CR_NAMESPACE_END
