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

#include <crlib/cr-basic.h>
#include <crlib/cr-string.h>

#if defined (CR_LINUX) || defined (CR_OSX)
#  include <dlfcn.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

CR_NAMESPACE_BEGIN

// handling dynamic library loading
class SharedLibrary final : public DenyCopying {
public:
   using Handle = void *;

private:
   Handle handle_ = nullptr;

public:
   explicit SharedLibrary () = default;

   SharedLibrary (StringRef file) {
      if (file.empty ()) {
         return;
      }
      load (file);
   }

   ~SharedLibrary () {
      unload ();
   }

public:
   bool load (StringRef file) noexcept {
      if (*this) {
         unload ();
      }

#if defined (CR_WINDOWS)
      handle_ = LoadLibraryA (file.chars ());
#elif defined (CR_OSX)
      handle_ = dlopen (file.chars (), RTLD_NOW | RTLD_LOCAL);
#else
      handle_ = dlopen (file.chars (), RTLD_NOW | RTLD_DEEPBIND | RTLD_LOCAL);
#endif
      return handle_ != nullptr;
   }

   bool locate (Handle address) {
#if defined (CR_WINDOWS)
      MEMORY_BASIC_INFORMATION mbi;

      if (!VirtualQuery (address, &mbi, sizeof (mbi))) {
         return false;
      }

      if (mbi.State != MEM_COMMIT) {
         return false;
      }
      handle_ = reinterpret_cast <Handle> (mbi.AllocationBase);
#else
      Dl_info dli;
      plat.bzero (&dli, sizeof (dli));

      if (dladdr (address, &dli)) {
         return load (dli.dli_fname);
      }
#endif
      return handle_ != nullptr;
   }

   void unload () noexcept {
      if (!*this) {
         return;
      }
#if defined (CR_WINDOWS)
      FreeLibrary (static_cast <HMODULE> (handle_));
#else
      dlclose (handle_);
#endif
      handle_ = nullptr;
   }

   template <typename R> R resolve (const char *function) const {
      if (!*this) {
         return nullptr;
      }
      return SharedLibrary::getSymbol <R> (handle (), function);
   }

   Handle handle () const {
      return handle_;
   }

public:
   explicit operator bool () const {
      return handle_ != nullptr;
   }

public:
  template <typename R> static inline R CR_STDCALL getSymbol (Handle module, const char *function) {
      return reinterpret_cast <R> (
#if defined (CR_WINDOWS)
         GetProcAddress (static_cast <HMODULE> (module), function)
#else
         dlsym (module, function)
#endif
         );
   }
};

CR_NAMESPACE_END
