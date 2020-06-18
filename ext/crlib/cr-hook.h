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

#if !defined (CR_WINDOWS)
#  include <sys/mman.h>
#endif

CR_NAMESPACE_BEGIN

class SimpleHook : DenyCopying {
private:
#if defined (CR_ARCH_X64)
   using uint = uint64;
#else
   using uint = uint32;
#endif

private:
   enum : uint32 {
#if defined (CR_ARCH_X64)
      CodeLength = 5 + sizeof (uint)
#else 
      CodeLength = 1 + sizeof (uint)
#endif
   };

private:
   bool patched_;

   uint pageSize_;
   uint originalFun_;
   uint hookedFun_;

   UniquePtr <uint8 []> originalBytes_;
   UniquePtr <uint8 []> hookedBytes_;

private:
   void setPageSize () {
#if defined (CR_WINDOWS)
      SYSTEM_INFO sysinfo;
      GetSystemInfo (&sysinfo);

      pageSize_ = sysinfo.dwPageSize;
#else 
      pageSize_ = sysconf (_SC_PAGESIZE);
#endif
}

#if !defined (CR_WINDOWS)
   void *align (void *address) {
      return reinterpret_cast <void *> ((reinterpret_cast <long> (address) & ~(pageSize_ - 1)));
   }
#endif

   bool unprotect () {
      auto orig = reinterpret_cast <void *> (originalFun_);

#if defined (CR_WINDOWS)
      DWORD oldProt;

      FlushInstructionCache (GetCurrentProcess (), orig, CodeLength);
      return VirtualProtect (orig, CodeLength, PAGE_EXECUTE_READWRITE, &oldProt);
#else 
      auto aligned = align (orig);
      return !mprotect (aligned, pageSize_, PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
   }

public:
   SimpleHook () : patched_ (false), pageSize_ (0), originalFun_ (0), hookedFun_ (0) {
      setPageSize ();

      originalBytes_ = makeUnique <uint8 []> (CodeLength);
      hookedBytes_ = makeUnique <uint8 []> (CodeLength);
   }

public:
   bool patch (void *address, void *replacement) {
      constexpr uint16 jmp = 0x25ff;

      if (plat.arm) {
         return false;
      }
      auto ptr = reinterpret_cast <uint8 *> (address);

      while (*reinterpret_cast <uint16 *> (ptr) == jmp) {
         ptr = **reinterpret_cast <uint8 ***> (ptr + 2);
      }
      originalFun_ = reinterpret_cast <uint> (ptr);
      hookedFun_ = reinterpret_cast <uint> (replacement);

      memcpy (originalBytes_.get (), reinterpret_cast <void *> (originalFun_), CodeLength);

      if (plat.x64) {
         const uint16 nop = 0x00000000;

         memcpy (&hookedBytes_[0], &jmp, sizeof (uint16));
         memcpy (&hookedBytes_[2], &nop, sizeof (uint16));
         memcpy (&hookedBytes_[6], &replacement, sizeof (uint));
      }
      else {
         hookedBytes_[0] = 0xe9;

         auto rel = hookedFun_ - originalFun_ - CodeLength;
         memcpy (&hookedBytes_[0] + 1, &rel, sizeof (rel));
      }
      return enable ();
   }

   bool enable () {
      if (patched_) {
         return false;
      }
      patched_ = true;

      if (unprotect ()) {
         memcpy (reinterpret_cast <void *> (originalFun_), hookedBytes_.get (), CodeLength);
         return true;
      }
      return false;
   }

   bool disable () {
      if (!patched_) {
         return false;
      }
      patched_ = false;

      if (unprotect ()) {
         memcpy (reinterpret_cast <void *> (originalFun_), originalBytes_.get (), CodeLength);
         return true;
      }
      return false;
   }

   bool enabled () const {
      return patched_;
   }

public:
   template <typename T, typename... Args > decltype (auto) call (Args &&...args) {
      disable ();
      auto res = reinterpret_cast <T *> (originalFun_) (args...);
      enable ();

      return res;
   }
};

CR_NAMESPACE_END
