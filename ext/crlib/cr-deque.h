//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2021 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <crlib/cr-memory.h>
#include <crlib/cr-twin.h>

CR_NAMESPACE_BEGIN

template <typename T> class Deque : private DenyCopying {
private:
   size_t capacity_ {};
   T *contents_ {};

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
      auto contents = Memory::get <T> (sizeof (T) * capacity);

      auto transfer = [] (T &dst, T &src) {
         Memory::construct (&dst, cr::move (src));
         Memory::destruct (&src);
      };

      if (index_.first < index_.second) {
         for (size_t i = 0; i < index_.second - index_.first; ++i) {
            transfer (contents[i], contents_[index_.first + i]);
         }
         index_.second = index_.second - index_.first;
         index_.first = 0;
      }
      else {
         for (size_t i = 0; i < capacity_ - index_.first; ++i) {
            transfer (contents[i], contents_[index_.first + i]);
         }

         for (size_t i = 0; i < index_.second; ++i) {
            transfer (contents[capacity_ - index_.first + i], contents_[i]);
         }
         index_.second = index_.second + (capacity_ - index_.first);
         index_.first = 0;
      }
      Memory::release (contents_);

      contents_ = contents;
      capacity_ = capacity;
   }

   void destroy () {
      auto destruct = [&] (size_t start, size_t end) {
         for (size_t i = start; i < end; ++i) {
            Memory::destruct (&contents_[i]);
         }
      };

      if (index_.first <= index_.second) {
         destruct (index_.first, index_.second);
      }
      else {
         destruct (index_.first, capacity_);
         destruct (0, index_.second);
      }
      Memory::release (contents_);
   }

   void reset () {
      contents_ = nullptr;
      capacity_ = 0;

      clear ();
   }

public:
   explicit Deque () : capacity_ (0), contents_ (nullptr)
   { }

   Deque (Deque &&rhs) : contents_ (rhs.contents_), capacity_ (rhs.capacity_) {
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

      Memory::construct (&contents_[index_.second], cr::forward <Args> (args)...);
      index_.second = rear;
   }

   template <typename ...Args> void emplaceFront (Args &&...args) {
      index_.first = pickFrontIndex ();
      Memory::construct (&contents_[index_.first], cr::forward <Args> (args)...);
   }

   void discardFront () {
      Memory::destruct (&contents_[index_.first]);

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
      Memory::destruct (&contents_[index_.second]);
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

      contents_ = rhs.contents_;
      capacity_ = rhs.capacity_;

      index_.first = rhs.index_.first;
      index_.second = rhs.index_.second;

      rhs.reset ();
      return *this;
   }
};

CR_NAMESPACE_END
