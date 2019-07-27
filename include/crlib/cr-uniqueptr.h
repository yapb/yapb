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
#include <crlib/cr-movable.h>

CR_NAMESPACE_BEGIN

// simple unique ptr
template <typename T> class UniquePtr final : public DenyCopying {
private:
   T *m_ptr = nullptr;

public:
   UniquePtr () = default;
   explicit UniquePtr (T *ptr) : m_ptr (ptr)
   { }

   UniquePtr (UniquePtr &&rhs) noexcept : m_ptr (rhs.m_ptr) {
      rhs.m_ptr = nullptr;
   }

   ~UniquePtr () {
      destroy ();
   }

public:
   T *get () const {
      return m_ptr;
   }

   T *release () {
      auto ret = m_ptr;
      m_ptr = nullptr;

      return ret;
   }

   void reset (T *ptr = nullptr) {
      destroy ();
      m_ptr = ptr;
   }

private:
   void destroy () {
      if (m_ptr) {
         alloc.destroy (m_ptr);
         m_ptr = nullptr;
      }
   }

public:
   UniquePtr &operator = (UniquePtr &&rhs) noexcept {
      if (this != &rhs) {
         destroy ();

         m_ptr = rhs.m_ptr;
         rhs.m_ptr = nullptr;
      }
      return *this;
   }

   UniquePtr &operator = (decltype (nullptr)) {
      destroy ();
      return *this;
   }

   T &operator * () const {
      return *m_ptr;
   }

   T *operator -> () const {
      return m_ptr;
   }

   explicit operator bool () const {
      return m_ptr != nullptr;
   }
};

// create unique
template <typename T, typename... Args> UniquePtr <T> createUnique (Args &&... args) {
   return UniquePtr <T> (alloc.create <T> (cr::forward <Args> (args)...));
}

// create unique (base class)
template <typename D, typename B, typename... Args> UniquePtr <B> createUniqueBase (Args &&... args) {
   return UniquePtr <B> (alloc.create <D> (cr::forward <Args> (args)...));
}

CR_NAMESPACE_END