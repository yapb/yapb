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

// configure export macros
#if defined(CR_WINDOWS)
#  define CR_EXPORT extern "C" __declspec (dllexport)
#elif defined(CR_LINUX) || defined(CR_OSX)
#  define CR_EXPORT extern "C" __attribute__((visibility("default")))
#else
#  error "Can't configure export macros. Compiler unrecognized."
#endif

#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) || defined(__amd64) || (defined(_MSC_VER) && defined(_M_X64))
#  define CR_ARCH_X64
#elif defined(__i686) || defined(__i686__) || defined(__i386) || defined(__i386__) || defined(i386) || (defined(_MSC_VER) && defined(_M_IX86))
#  define CR_ARCH_X86
#endif

#if (defined(CR_ARCH_X86) || defined(CR_ARCH_X64)) && !defined(CR_DEBUG)
#  define CR_HAS_SSE2
#endif

CR_NAMESPACE_END

#if defined(CR_WINDOWS)
#  include <direct.h>
#  define stricmp _stricmp
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  define stricmp strcasecmp
#endif

#include <assert.h>

#if defined (CR_ANDROID)
#  include <android/log.h>
#endif

CR_NAMESPACE_BEGIN

// helper struct for platform detection
struct Platform : public Singleton <Platform> {
   bool isWindows = false;
   bool isLinux = false;
   bool isOSX = false;
   bool isAndroid = false;
   bool isAndroidHardFP = false;
   bool isX64 = false;

   Platform () {
#if defined(CR_WINDOWS)
      isWindows = true;
#endif

#if defined(CR_ANDROID)
      isAndroid = true;

#     if defined (CR_ANDROID_HARD_FP)
         isAndroidHardFP = true;
#     endif
#endif

#if defined(CR_LINUX)
      isLinux = true;
#endif

#if defined (CR_OSX)
      isOSX = true;
#endif

#if defined (CR_ARCH_X64)
      isX64 = true;
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