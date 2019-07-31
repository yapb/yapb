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

#if !defined (CR_WINDOWS)
#  include <sys/mman.h>
#endif

CR_NAMESPACE_BEGIN

class SimpleHook : DenyCopying {
private:
   enum : uint32 {
      CodeLength = 12
   };

private:
   bool m_patched;

   uint32 m_pageSize;
   uint32 m_origFunc;
   uint32 m_hookFunc;

   uint8 m_origBytes[CodeLength] {};
   uint8 m_hookBytes[CodeLength] {};

private:
   void setPageSize () {
#if defined (CR_WINDOWS)
      SYSTEM_INFO sysinfo;
      GetSystemInfo (&sysinfo);

      m_pageSize = sysinfo.dwPageSize;
#else 
      m_pageSize = sysconf (_SC_PAGESIZE);
#endif
   }

   inline void *align (void *address) {
      return reinterpret_cast <void *> ((reinterpret_cast <long> (address) & ~(m_pageSize - 1)));
   }

   bool unprotect () {
      auto orig = reinterpret_cast <void *> (m_origFunc);

#if defined (CR_WINDOWS)
      DWORD oldProt;

      FlushInstructionCache (GetCurrentProcess (), orig, m_pageSize);
      return VirtualProtect (orig, m_pageSize, PAGE_EXECUTE_READWRITE, &oldProt);
#else 
      auto aligned = align (orig);
      return !mprotect (aligned, m_pageSize, PROT_READ | PROT_WRITE | PROT_EXEC);
#endif
   }

public:
   SimpleHook () : m_patched (false), m_pageSize (0), m_origFunc (0), m_hookFunc (0) {
      setPageSize ();
   }

   ~SimpleHook () {
      disable ();
   }

public:
   bool patch (void *address, void *replacement) {
      uint8 *ptr = reinterpret_cast <uint8 *> (address);

      while (*reinterpret_cast <uint16 *> (ptr) == 0x25ff) {
         ptr = **reinterpret_cast <uint8 * **> ((ptr + 2));
      }
      m_origFunc = reinterpret_cast <uint32> (ptr);

      if (!m_origFunc) {
         return false;
      }
      m_hookFunc = reinterpret_cast <uint32> (replacement);

      m_hookBytes[0] = 0x68;
      m_hookBytes[5] = 0xc3;

      *reinterpret_cast <uint32 *> (&m_hookBytes[1]) = m_hookFunc;

      if (unprotect ()) {
         memcpy (m_origBytes, reinterpret_cast <void *> (m_origFunc), CodeLength);
         return enable ();
      }
      return false;
   }

   bool enable () {
      if (m_patched) {
         return false;
      }
      m_patched = true;

      if (unprotect ()) {
         memcpy (reinterpret_cast <void *> (m_origFunc), m_hookBytes, CodeLength);
         return true;
      }
      return false;
   }

   bool disable () {
      if (!m_patched) {
         return false;
      }
      m_patched = false;

      if (unprotect ()) {
         memcpy (reinterpret_cast <void *> (m_origFunc), m_origBytes, CodeLength);
         return true;
      }
      return false;
   }

   bool enabled () const {
      return m_patched;
   }
};

CR_NAMESPACE_END