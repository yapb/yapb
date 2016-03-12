//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#pragma once

// detects the build platform
#if defined (__linux__) || defined (__debian__) || defined (__linux)
   #define PLATFORM_LINUX 1
#elif defined (__APPLE__)
   #define PLATFORM_OSX 1
#elif defined (_WIN32)
   #define PLATFORM_WIN32 1
#endif

// detects the compiler
#if defined (_MSC_VER)
   #define COMPILER_VISUALC _MSC_VER
#elif defined (__MINGW32_MAJOR_VERSION)
   #define COMPILER_MINGW32 __MINGW32_MAJOR_VERSION
#endif

// configure export macros
#if defined (COMPILER_VISUALC) || defined (COMPILER_MINGW32)
   #define SHARED_LIBRARAY_EXPORT extern "C" __declspec (dllexport)
#elif defined (PLATFORM_LINUX) || defined (PLATFORM_OSX) 
   #define SHARED_LIBRARAY_EXPORT extern "C" __attribute__((visibility("default")))
#else
   #error "Can't configure export macros. Compiler unrecognized."
#endif

// operating system specific macros, functions and typedefs
#ifdef PLATFORM_WIN32

   #include <direct.h>
   #include <string.h>

   #define DLL_ENTRYPOINT int STDCALL DllMain (HINSTANCE, DWORD dwReason, LPVOID)
   #define DLL_DETACHING (dwReason == DLL_PROCESS_DETACH)
   #define DLL_RETENTRY return TRUE

   #if defined (COMPILER_VISUALC)
      #define DLL_GIVEFNPTRSTODLL extern "C" void STDCALL
   #elif defined (COMPILER_MINGW32)
      #define DLL_GIVEFNPTRSTODLL SHARED_LIBRARAY_EXPORT void STDCALL
   #endif

   // specify export parameter
   #if defined (COMPILER_VISUALC) && (COMPILER_VISUALC > 1000)
      #pragma comment (linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8,@1")
      #pragma comment (linker, "/SECTION:.data,RW")
   #endif

   typedef int (FAR *EntityAPI_t) (gamefuncs_t *, int);
   typedef int (FAR *NewEntityAPI_t) (newgamefuncs_t *, int *);
   typedef int (FAR *BlendAPI_t) (int, void **, void *, float (*)[3][4], float (*)[128][3][4]);
   typedef void (STDCALL *FuncPointers_t) (enginefuncs_t *, globalvars_t *);
   typedef void (FAR *EntityPtr_t) (entvars_t *);

#elif defined (PLATFORM_LINUX) || defined (PLATFORM_OSX)

   #include <unistd.h>
   #include <dlfcn.h>
   #include <errno.h>
   #include <fcntl.h>
   #include <sys/stat.h>

   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>

   #define DLL_ENTRYPOINT __attribute__((destructor))  void _fini (void)
   #define DLL_DETACHING TRUE
   #define DLL_RETENTRY return
   #define DLL_GIVEFNPTRSTODLL extern "C" void __attribute__((visibility("default")))

   #if defined (__ANDROID__)
   #define PLATFORM_ANDROID 1
   #endif

   typedef int (*EntityAPI_t) (gamefuncs_t *, int);
   typedef int (*NewEntityAPI_t) (newgamefuncs_t *, int *);
   typedef int (*BlendAPI_t) (int, void **, void *, float (*)[3][4], float (*)[128][3][4]);
   typedef void (*FuncPointers_t) (enginefuncs_t *, globalvars_t *);
   typedef void (*EntityPtr_t) (entvars_t *);

#else
   #error "Platform unrecognized."
#endif

// library wrapper
class Library
{
private:
   void *m_ptr;

public:

   Library (const char *fileName)
   {
      m_ptr = NULL;

      if (fileName == NULL)
         return;

      LoadLib (fileName);
   }

   ~Library (void)
   {
      if (!IsLoaded ())
         return;

#ifdef PLATFORM_WIN32
      FreeLibrary ((HMODULE) m_ptr);
#else
      dlclose (m_ptr);
#endif
   }

public:
   inline void *LoadLib (const char *fileName)
   {
#ifdef PLATFORM_WIN32
      m_ptr = LoadLibrary (fileName);
#else
      m_ptr = dlopen (fileName, RTLD_NOW);
#endif

      return m_ptr;
   }

   inline void *GetFuncAddr (const char *function)
   {
#ifdef PLATFORM_WIN32
      return reinterpret_cast <void *> (GetProcAddress (static_cast <HMODULE> (m_ptr), function));
#else
      return reinterpret_cast <void *> (dlsym (m_ptr, function));
#endif
   }

   inline void *GetHandle (void)
   {
      return m_ptr;
   }

   inline bool IsLoaded (void) const
   {
      return m_ptr != NULL;
   }
};