//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

// detects the build platform
#if defined(__linux__) || defined(__debian__) || defined(__linux)
#define PLATFORM_LINUX 1
#elif defined(__APPLE__)
#define PLATFORM_OSX 1
#elif defined(_WIN32)
#define PLATFORM_WIN32 1
#endif

// detects the compiler
#if defined(_MSC_VER)
#define COMPILER_VISUALC _MSC_VER
#elif defined(__MINGW32_MAJOR_VERSION)
#define COMPILER_MINGW32 __MINGW32_MAJOR_VERSION
#endif

// configure export macros
#if defined(COMPILER_VISUALC) || defined(COMPILER_MINGW32)
#define SHARED_LIBRARAY_EXPORT extern "C" __declspec(dllexport)
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_OSX)
#define SHARED_LIBRARAY_EXPORT extern "C" __attribute__ ((visibility ("default")))
#else
#error "Can't configure export macros. Compiler unrecognized."
#endif

// enable sse intrinsics
#define ENABLE_SSE_INTRINSICS 1

// operating system specific macros, functions and typedefs
#ifdef PLATFORM_WIN32

#include <direct.h>
#include <string.h>

#define STD_CALL __stdcall

#define DLL_ENTRYPOINT int STD_CALL DllMain (HINSTANCE, DWORD dwReason, LPVOID)
#define DLL_DETACHING (dwReason == DLL_PROCESS_DETACH)
#define DLL_RETENTRY return TRUE

#if defined(COMPILER_VISUALC)
#define DLL_GIVEFNPTRSTODLL extern "C" void STD_CALL
#elif defined(COMPILER_MINGW32)
#define DLL_GIVEFNPTRSTODLL SHARED_LIBRARAY_EXPORT void STD_CALL
#endif

// specify export parameter
#if defined(COMPILER_VISUALC) && (COMPILER_VISUALC > 1000)
#pragma comment(linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8,@1")
#pragma comment(linker, "/SECTION:.data,RW")
#endif

#elif defined(PLATFORM_LINUX) || defined(PLATFORM_OSX)

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define DLL_ENTRYPOINT __attribute__ ((destructor)) void _fini (void)
#define DLL_DETACHING TRUE
#define DLL_RETENTRY return
#define DLL_GIVEFNPTRSTODLL extern "C" void __attribute__ ((visibility ("default")))

#define STD_CALL /* */

#if defined(__ANDROID__)
#define PLATFORM_ANDROID 1
#undef ENABLE_SSE_INTRINSICS
#endif
#else
#error "Platform unrecognized."
#endif
