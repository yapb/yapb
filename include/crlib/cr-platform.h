//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
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
#  define CR_CXX_MSVC
#elif defined(__clang__)
#  define CR_CXX_CLANG
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
#  define CR_HAS_SSE2
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

   bool caseStrMatch (const char *str1, const char *str2) {
#if defined(CR_WINDOWS)
      return _stricmp (str1, str2) == 0;
#else
      return ::strcasecmp (str1, str2) == 0;
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
      if (msg) {
         DestroyWindow (GetForegroundWindow ());
         MessageBoxA (GetActiveWindow (), msg, "crlib.fatal", MB_ICONSTOP);
      }
#endif
      ::abort ();
   }
};

// expose platform singleton
static auto &plat = Platform::get ();

CR_NAMESPACE_END