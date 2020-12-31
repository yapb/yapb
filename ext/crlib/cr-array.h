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
#include <crlib/cr-random.h>

#include <initializer_list>

// policy to reserve memory
CR_DECLARE_SCOPED_ENUM (ReservePolicy,
   Multiple,
   Single,
)

CR_NAMESPACE_BEGIN

// simple array class like std::vector
template <typename T, ReservePolicy R = ReservePolicy::Multiple, size_t S = 0> class Array : public DenyCopying {
private:
   T *contents_ {};
   size_t capacity_ {};
   size_t length_ {};

public:
   explicit Array () {
      if (fix (S > 0)) {
         reserve (S);
      }
   }

   Array (const size_t amount) {
      reserve (amount);
   }

   Array (Array &&rhs) noexcept {
      contents_ = rhs.contents_;
      length_ = rhs.length_;
      capacity_ = rhs.capacity_;

      rhs.reset ();
   }

   Array (const std::initializer_list <T> &list) {
      for (const auto &elem : list) {
         push (elem);
      }
   }

   ~Array () {
      destroy ();
   }

private:
   void destructElements () noexcept {
      for (size_t i = 0; i < length_; ++i) {
         Memory::destruct (&contents_[i]);
      }
   }

   void transferElements (T *dest, T *src, size_t length) noexcept {
      for (size_t i = 0; i < length; ++i) {
         Memory::construct (&dest[i], cr::move (src[i]));
         Memory::destruct (&src[i]);
      }
   }

   void destroy () {
      destructElements ();
      Memory::release (contents_);
   }

   void reset () {
      contents_ = nullptr;
      capacity_ = 0;
      length_ = 0;
   }

public:
   bool reserve (const size_t amount) {
      if (length_ + amount < capacity_) {
         return true;
      }
      auto capacity = capacity_ ? capacity_ : 12;

      if (cr::fix (R == ReservePolicy::Multiple)) {
         while (length_ + amount > capacity) {
            capacity *= 2;
         }
      }
      else {
         capacity = amount + capacity_ + 1;
      }
      auto data = Memory::get <T> (capacity);

      if (contents_) {
         transferElements (data, contents_, length_);
         Memory::release (contents_);
      }

      contents_ = data;
      capacity_ = capacity;

      return true;
   }

   bool resize (const size_t amount) {
      if (amount < length_) {
         while (amount < length_) {
            discard ();
         }
      }
      else if (amount > length_) {
         if (!ensure (amount)) {
            return false;
         }
         size_t resizeLength = amount - length_;

         while (resizeLength--) {
            emplace ();
         }
      }
      return true;
   }

   bool ensure (const size_t amount) {
      if (amount <= length_) {
         return true;
      }
      return reserve (amount - length_);
   }

   template <typename U = size_t> U length () const {
      return static_cast <U> (length_);
   }

   size_t capacity () const {
      return capacity_;
   }

   template <typename U> bool set (size_t index, U &&object) {
      if (index >= capacity_) {
         if (!reserve (index + 1)) {
            return false;
         }
      }
      Memory::construct (&contents_[index], cr::forward <U> (object));

      if (index >= length_) {
         length_ = index + 1;
      }
      return true;
   }

   template <typename U> bool insert (size_t index, U &&object) {
      return insert (index, &object, 1);
   }

   template <typename U> bool insert (size_t index, U *objects, size_t count = 1) {
      if (!objects || !count) {
         return false;
      }
      const size_t capacity = (length_ > index ? length_ : index) + count;

      if (capacity >= capacity_ && !reserve (capacity)) {
         return false;
      }

      if (index >= length_) {
         for (size_t i = 0; i < count; ++i) {
            Memory::construct (&contents_[i + index], cr::forward <U> (objects[i]));
         }
         length_ = capacity;
      }
      else {
         size_t i = 0;

         for (i = length_; i > index; --i) {
            contents_[i + count - 1] = cr::move (contents_[i - 1]);
         }
         for (i = 0; i < count; ++i) {
            Memory::construct (&contents_[i + index], cr::forward <U> (objects[i]));
         }
         length_ += count;
      }
      return true;
   }

   bool insert (size_t at, const Array &rhs) {
      if (&rhs == this) {
         return false;
      }
      return insert (at, &rhs.contents_[0], rhs.length_);
   }

   bool erase (const size_t index, const size_t count) {
      if (index + count > capacity_) {
         return false;
      }
      for (size_t i = index; i < index + count; ++i) {
         Memory::destruct (&contents_[i]);
      }
      length_ -= count;

      for (size_t i = index; i < length_; ++i) {
         contents_[i] = cr::move (contents_[i + count]);
      }
      return true;
   }

   bool shift () {
      return erase (0, 1);
   }

   template <typename U> bool unshift (U &&object) {
      return insert (0, &object);
   }

   bool remove (const T &object) {
      return erase (index (object), 1);
   }

   template <typename U> bool push (U &&object) {
      if (!reserve (1)) {
         return false;
      }
      Memory::construct (&contents_[length_], cr::forward <U> (object));
      ++length_;

      return true;
   }

   template <typename ...Args> bool emplace (Args &&...args) {
      if (!reserve (1)) {
         return false;
      }
      Memory::construct (&contents_[length_], cr::forward <Args> (args)...);
      ++length_;

      return true;
   }

   T pop () {
      auto object = cr::move (contents_[length_ - 1]);
      discard ();

      return object;
   }

   void discard () {
      erase (length_ - 1, 1);
   }

   size_t index (const T &object) const {
      return &object - &contents_[0];
   }

   void shuffle () {
      int32 shuffleLength = length <int32> ();

      for (int32 i = shuffleLength; i >= 1; --i) {
         cr::swap (contents_[i - 1], contents_[rg.get (i, shuffleLength - 2)]);
      }
   }

   void reverse () {
      for (size_t i = 0; i < length_ / 2; ++i) {
         cr::swap (contents_[i], contents_[length_ - 1 - i]);
      }
   }

   template <typename U> bool extend (U &&rhs) {
      if (length_ == 0) {
         *this = cr::move (rhs);
      }
      else {
         for (size_t i = 0; i < rhs.length (); ++i) {
            if (!push (cr::move (rhs[i]))) {
               return false;
            }
         }
      }
      return true;
   }

   template <typename U> bool assign (U &&rhs) {
      clear ();
      return extend (cr::move (rhs));
   }

   void clear () {
      destructElements ();
      length_ = 0;
   }

   bool empty () const {
      return length_ == 0;
   }

   bool shrink () {
      if (length_ == capacity_ || !length_) {
         return false;
      }

      auto data = Memory::get <T> (length_);
      transferElements (data, contents_, length_);

      Memory::release (contents_);

      contents_ = data;
      capacity_ = length_;

      return true;
   }

   const T &at (size_t index) const {
      return contents_[index];
   }

   T &at (size_t index) {
      return contents_[index];
   }

   const T &first () const {
      return contents_[0];
   }

   T &first () {
      return contents_[0];
   }

   T &last () {
      return contents_[length_ - 1];
   }

   const T &last () const {
      return contents_[length_ - 1];
   }

   const T &random () const {
      return contents_[rg.get (0, length <int32> () - 1)];
   }

   T &random () {
      return contents_[rg.get (0, length <int32> () - 1)];
   }

   T *data () {
      return contents_;
   }

   T *data () const {
      return contents_;
   }

public:
   Array &operator = (Array &&rhs) noexcept {
      if (this != &rhs) {
         destroy ();

         contents_ = rhs.contents_;
         length_ = rhs.length_;
         capacity_ = rhs.capacity_;

         rhs.reset ();
      }
      return *this;
   }

public:
   const T &operator [] (size_t index) const {
      return at (index);
   }

   T &operator [] (size_t index) {
      return at (index);
   }

   // for range-based loops
public:
   T *begin () {
      return contents_;
   }

   T *begin () const {
      return contents_;
   }

   T *end () {
      return contents_ + length_;
   }

   T *end () const {
      return contents_ + length_;
   }
};

// small array (with minimal reserve policy, something like fixed array, but still able to grow, by default allocates 64 elements)
template <typename T> using SmallArray = Array <T, ReservePolicy::Single, 64>;

CR_NAMESPACE_END
