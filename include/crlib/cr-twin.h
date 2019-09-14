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

// simple pair (twin)
template <typename A, typename B> class Twin final {
public:
   A first;
   B second;

public:
   template <typename T, typename U> Twin (T &&a, U &&b) : first (cr::forward <T> (a)), second (cr::forward <U> (b)) { }
   template <typename T, typename U> Twin (const Twin <T, U> &rhs) : first (rhs.first), second (rhs.second) { }
   template <typename T, typename U> Twin (Twin <T, U> &&rhs) noexcept : first (cr::move (rhs.first)), second (cr::move (rhs.second)) { }

public:
   explicit Twin () = default;
   ~Twin () = default;

public:
   template <typename T, typename U> Twin &operator = (const Twin <T, U> &rhs) {
      first = rhs.first;
      second = rhs.second;

      return *this;
   }

   template <typename T, typename U> Twin &operator = (Twin <T, U> &&rhs) {
      first = cr::move (rhs.first);
      second = cr::move (rhs.second);

      return *this;
   }

   // specialized operators for binary heap, do not use as it's test only second element
public:
   friend bool operator < (const Twin &a, const Twin &b) {
      return a.second < b.second;
   }

   friend bool operator > (const Twin &a, const Twin &b) {
      return b.second < a.second;
   }
};

CR_NAMESPACE_END