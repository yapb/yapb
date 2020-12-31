//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2021 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-basic.h>
#include <crlib/cr-memory.h>
#include <crlib/cr-movable.h>

CR_NAMESPACE_BEGIN

// simple unique ptr
template <typename T> class UniquePtr final : public DenyCopying {
private:
   T *ptr_ {};

public:
   UniquePtr () = default;

   explicit UniquePtr (T *ptr) : ptr_ (ptr)
   { }

   UniquePtr (UniquePtr &&rhs) noexcept : ptr_ (rhs.release ())
   { }

   template <typename U> UniquePtr (UniquePtr <U> &&rhs) noexcept : ptr_ (rhs.release ())
   { }

   ~UniquePtr () {
      destroy ();
   }

public:
   T *get () const {
      return ptr_;
   }

   T *release () {
      auto ret = ptr_;
      ptr_ = nullptr;

      return ret;
   }

   void reset (T *ptr = nullptr) {
      destroy ();
      ptr_ = ptr;
   }

private:
   void destroy () {
      delete ptr_;
      ptr_ = nullptr;
   }

public:
   UniquePtr &operator = (UniquePtr &&rhs) noexcept {
      if (this != &rhs) {
         reset (rhs.release ());
      }
      return *this;
   }

   template <typename U> UniquePtr &operator = (UniquePtr <U> &&rhs) noexcept {
      if (this != &rhs) {
         reset (rhs.release ());
      }
      return *this;
   }

   UniquePtr &operator = (decltype (nullptr)) {
      destroy ();
      return *this;
   }

   T &operator * () const {
      return *ptr_;
   }

   T *operator -> () const {
      return ptr_;
   }

   explicit operator bool () const {
      return ptr_ != nullptr;
   }
};

template <typename T> class UniquePtr <T[]> final : public DenyCopying {
private:
   T *ptr_ { };

public:
   UniquePtr () = default;

   explicit UniquePtr (T *ptr) : ptr_ (ptr)
   { }

   UniquePtr (UniquePtr &&rhs) noexcept : ptr_ (rhs.release ())
   { }

   template <typename U> UniquePtr (UniquePtr <U> &&rhs) noexcept : ptr_ (rhs.release ())
   { }

   ~UniquePtr () {
      destroy ();
   }

public:
   T *get () const {
      return ptr_;
   }

   T *release () {
      auto ret = ptr_;
      ptr_ = nullptr;

      return ret;
   }

   void reset (T *ptr = nullptr) {
      destroy ();
      ptr_ = ptr;
   }

private:
   void destroy () {
      delete[] ptr_;
      ptr_ = nullptr;
   }

public:
   UniquePtr &operator = (UniquePtr &&rhs) noexcept {
      if (this != &rhs) {
         reset (rhs.release ());
      }
      return *this;
   }

   template <typename U> UniquePtr &operator = (UniquePtr <U> &&rhs) noexcept {
      if (this != &rhs) {
         reset (rhs.release ());
      }
      return *this;
   }

   UniquePtr &operator = (decltype (nullptr)) {
      destroy ();
      return *this;
   }

   T &operator [] (size_t index) {
      return ptr_[index];
   }

   const T &operator [] (size_t index) const {
      return ptr_[index];
   }

   explicit operator bool () const {
      return ptr_ != nullptr;
   }
};

namespace detail {
   template <typename T> struct ClearExtent {
      using Type = T;
   };

   template <typename T> struct ClearExtent <T[]> {
      using Type = T;
   };

   template <typename T, size_t N> struct ClearExtent <T[N]> {
      using Type = T;
   };

   template <typename T> struct UniqueIf {
      using SingleObject = UniquePtr <T>;
   };

   template <typename T> struct UniqueIf<T[]> {
      using UnknownBound = UniquePtr <T[]>;
   };

   template <typename T, size_t N> struct UniqueIf <T[N]> {
      using KnownBound = void;
   };
}

template <typename T, typename... Args> typename detail::UniqueIf <T>::SingleObject makeUnique (Args &&... args) {
   return UniquePtr <T> (new T (cr::forward <Args> (args)...));
}

template <typename T> typename detail::UniqueIf <T>::UnknownBound makeUnique (size_t size) {
   using Type = typename detail::ClearExtent <T>::Type;
   return UniquePtr<T> (new Type[size] ());
}

template <typename T, typename... Args> typename detail::UniqueIf <T>::KnownBound makeUnique (Args &&...) = delete;

CR_NAMESPACE_END
