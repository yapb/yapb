//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
//

#pragma once

#include <crlib/cr-basic.h>

CR_NAMESPACE_BEGIN

namespace detail {
   template <typename T> struct ClearRef {
      using Type = T;
   };
   template <typename T> struct ClearRef <T &> {
      using Type = T;
   };
   template <typename T> struct ClearRef <T &&> {
      using Type = T;
   };
}

template <typename T> typename detail::ClearRef <T>::Type constexpr &&move (T &&type) noexcept {
   return static_cast <typename detail::ClearRef <T>::Type &&> (type);
}

template <typename T> constexpr T &&forward (typename detail::ClearRef <T>::Type &type) noexcept {
   return static_cast <T &&> (type);
}

template <typename T> constexpr T &&forward (typename detail::ClearRef <T>::Type &&type) noexcept {
   return static_cast <T &&> (type);
}

template <typename T> inline void swap (T &left, T &right) noexcept {
   auto temp = cr::move (left);
   left = cr::move (right);
   right = cr::move (temp);
}

template <typename T, size_t S> inline void swap (T (&left)[S], T (&right)[S]) noexcept {
   if (&left == &right) {
      return;
   }
   auto begin = left;
   auto end = begin + S;

   for (auto temp = right; begin != end; ++begin, ++temp) {
      cr::swap (*begin, *temp);
   }
}

CR_NAMESPACE_END