//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <crlib/cr-basic.h>
#include <crlib/cr-string.h>

#if defined (CR_LINUX) || defined (CR_OSX)
#  include <dlfcn.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#elif defined (CR_WINDOWS)
#  define WIN32_LEAN_AND_MEAN 
#  include <windows.h>
#endif


CR_NAMESPACE_BEGIN

// handling dynamic library loading
class SharedLibrary final : public DenyCopying {
private:
   void *m_handle = nullptr;

public:
   explicit SharedLibrary () = default;

   SharedLibrary (const String &file) {
      if (file.empty ()) {
         return;
      }
      load (file);
   }

   ~SharedLibrary () {
      unload ();
   }

public:
   bool load (const String &file) noexcept {
#if defined (CR_WINDOWS)
      m_handle = LoadLibraryA (file.chars ());
#else
      m_handle = dlopen (file.chars (), RTLD_NOW);
#endif
      return m_handle != nullptr;
   }

   bool locate (void *address) {
#if defined (CR_WINDOWS)
      MEMORY_BASIC_INFORMATION mbi;

      if (!VirtualQuery (address, &mbi, sizeof (mbi))) {
         return false;
      }

      if (mbi.State != MEM_COMMIT) {
         return false;
      }
      m_handle = reinterpret_cast <void *> (mbi.AllocationBase);
#else
      Dl_info dli;
      memset (&dli, 0, sizeof (dli));

      if (dladdr (address, &dli)) {
         return load (dli.dli_fname);
      }
#endif
      return m_handle != nullptr;
   }

   void unload () noexcept {
      if (!*this) {
         return;
      }
#if defined (CR_WINDOWS)
      FreeLibrary (static_cast <HMODULE> (m_handle));
#else
      dlclose (m_handle);
#endif
      m_handle = nullptr;
   }

   template <typename R> R resolve (const char *function) const {
      if (!*this) {
         return nullptr;
      }

      return reinterpret_cast <R> (
#if defined (CR_WINDOWS)
         GetProcAddress (static_cast <HMODULE> (m_handle), function)
#else
         dlsym (m_handle, function)
#endif
         );
   }

   void *handle () const {
      return m_handle;
   }

public:
   explicit operator bool () const {
      return m_handle != nullptr;
   }
};

CR_NAMESPACE_END