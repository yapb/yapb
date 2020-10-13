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

#include <crlib/cr-alloc.h>
#include <crlib/cr-uniqueptr.h>
#include <crlib/cr-twin.h>

CR_NAMESPACE_BEGIN

template <typename T> class Deque : private DenyCopying {
private:
   size_t capacity_ {};

   UniquePtr <T[]> contents_ {};
   Twin <size_t, size_t> index_ {};

private:
   size_t pickFrontIndex () {
      if (index_.first == 0) {
         if (capacity_ && index_.second != capacity_ - 1) {
            return capacity_ - 1;
         }
      }
      else if (index_.first - 1 != index_.second) {
         return index_.first - 1;
      }
      extendCapacity ();

      return capacity_ - 1;
   }

   size_t pickRearIndex () {
      if (index_.second < index_.first) {
         if (index_.second + 1 != index_.first) {
            return index_.second + 1;
         }
      }
      else {
         if (index_.second + 1 < capacity_) {
            return index_.second + 1;
         }
         if (index_.first != 0) {
            return 0;
         }
      }
      extendCapacity ();

      return index_.second + 1;
   }

   void extendCapacity () {
      auto capacity = capacity_ ? capacity_ * 2 : 8;
      auto contents = cr::makeUnique <T[]> (sizeof (T) * capacity);

      if (index_.first < index_.second) {
         for (size_t i = 0; i < index_.second - index_.first; ++i) {
            contents[i] = cr::move (contents_[index_.first + i]);
         }
         index_.second = index_.second - index_.first;
         index_.first = 0;
      }
      else {
         for (size_t i = 0; i < capacity_ - index_.first; ++i) {
            contents[i] = cr::move (contents_[index_.first + i]);
         }

         for (size_t i = 0; i < index_.second; ++i) {
            contents[capacity_ - index_.first + i] = cr::move (contents_[i]);
         }
         index_.second = index_.second + (capacity_ - index_.first);
         index_.first = 0;
      }
      contents_ = cr::move (contents);
      capacity_ = capacity;
   }

   void destroy () {
      auto destruct = [&] (size_t start, size_t end) {
         for (size_t i = start; i < end; ++i) {
            alloc.destruct (&contents_[i]);
         }
      };

      if (index_.first <= index_.second) {
         destruct (index_.first, index_.second);
      }
      else {
         destruct (index_.first, capacity_);
         destruct (0, index_.second);
      }
      contents_ = nullptr;
   }

   void reset () {
      contents_ = nullptr;
      capacity_ = 0;

      clear ();
   }

public:
   explicit Deque () : capacity_ (0), contents_ (nullptr)
   { }

   Deque (Deque &&rhs) : contents_ (cr::move (rhs.contents_)), capacity_ (rhs.capacity_) {
      index_.first (rhs.index_.first);
      index_.second (rhs.index_.second);

      rhs.reset ();
   }

   ~Deque () {
      destroy ();
   }

public:
   bool empty () const {
      return index_.first == index_.second;
   }

   template <typename ...Args> void emplaceLast (Args &&...args) {
      auto rear = pickRearIndex ();

      alloc.construct (&contents_[index_.second], cr::forward <Args> (args)...);
      index_.second = rear;
   }

   template <typename ...Args> void emplaceFront (Args &&...args) {
      index_.first = pickFrontIndex ();
      alloc.construct (&contents_[index_.first], cr::forward <Args> (args)...);
   }

   void discardFront () {
      alloc.destruct (&contents_[index_.first]);

      if (index_.first == capacity_ - 1) {
         index_.first = 0;
      }
      else {
         index_.first++;
      }
   }

   void discardLast () {
      if (index_.second == 0) {
         index_.second = capacity_ - 1;
      }
      else {
         index_.second--;
      }
      alloc.destruct (&contents_[index_.second]);
   }

   T popFront () {
      auto object (front ());
      discardFront ();

      return object;
   }

   T popLast () {
      auto object (last ());
      discardLast ();

      return object;
   }

public:
   const T &front () const {
      return contents_[index_.first];
   }

   const T &last () const {
      if (index_.second == 0) {
         return contents_[capacity_ - 1];
      }
      return contents_[index_.second - 1];
   }

   T &front () {
      return contents_[index_.first];
   }

   T &last () {
      if (index_.second == 0) {
         return contents_[capacity_ - 1];
      }
      return contents_[index_.second - 1];
   }

   size_t length () const {
      if (index_.first == index_.second) {
         return 0;
      }
      return index_.first < index_.second ? index_.second - index_.first : index_.second + (capacity_ - index_.first);
   }

   void clear () {
      index_.first = 0;
      index_.second = 0;
   }

public:
   Deque &operator = (Deque &&rhs) {
      destroy ();

      contents_ = cr::move (rhs.contents_);
      capacity_ = rhs.capacity_;

      index_.first = rhs.index_.first;
      index_.second = rhs.index_.second;

      rhs.reset ();
      return *this;
   }
};

CR_NAMESPACE_END
