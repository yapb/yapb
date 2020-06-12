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
