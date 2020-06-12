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

#include <crlib/cr-array.h>

CR_NAMESPACE_BEGIN

// simple priority queue
template <typename T> class BinaryHeap final : public DenyCopying {
private:
   Array <T> contents_;

public:
   explicit BinaryHeap () = default;

   BinaryHeap (BinaryHeap &&rhs) noexcept : contents_ (cr::move (rhs.contents_))
   { }

   ~BinaryHeap () = default;

public:
   template <typename U> bool push (U &&item) {
      if (!contents_.push (cr::move (item))) {
         return false;
      }
      const size_t length = contents_.length ();

      if (length > 1) {
         percolateUp (length - 1);
      }
      return true;
   }

   template <typename ...Args> bool emplace (Args &&...args) {
      if (!contents_.emplace (cr::forward <Args> (args)...)) {
         return false;
      }
      const size_t length = contents_.length ();

      if (length > 1) {
         percolateUp (length - 1);
      }
      return true;
   }

   const T &top () const {
      return contents_[0];
   }

   T pop () {
      if (contents_.length () == 1) {
         return contents_.pop ();
      }
      auto key (cr::move (contents_[0]));

      contents_[0] = cr::move (contents_.last ());
      contents_.discard ();

      percolateDown (0);
      return key;
   }

public:
   size_t length () const {
      return contents_.length ();
   }

   bool empty () const {
      return !contents_.length ();
   }

   void clear () {
      contents_.clear ();
   }

private:
   void percolateUp (size_t index) {
      while (index != 0) {
         const size_t parentIndex = parent (index);

         if (contents_[parentIndex] > contents_[index]) {
            cr::swap (contents_[index], contents_[parentIndex]);
            index = parentIndex;
         }
         else {
            break;
         }
      }
   }

   void percolateDown (size_t index) {
      while (hasLeft (index)) {
         size_t bestIndex = left (index);

         if (hasRight (index)) {
            const size_t rightIndex = right (index);

            if (contents_[rightIndex] < contents_[bestIndex]) {
               bestIndex = rightIndex;
            }
         }

         if (contents_[index] > contents_[bestIndex]) {
            cr::swap (contents_[index], contents_[bestIndex]);

            index = bestIndex;
            bestIndex = left (index);
         }
         else {
            break;
         }
      }
   }

private:
   BinaryHeap &operator = (BinaryHeap &&rhs) noexcept {
      if (this != &rhs) {
         contents_ = cr::move (rhs.contents_);
      }
      return *this;
   }

private:
   static constexpr size_t parent (size_t index) {
      return (index - 1) / 2;
   }

   static constexpr size_t left (size_t index) {
      return (index * 2) + 1;
   }

   static constexpr size_t right (size_t index) {
      return (index * 2) + 2;
   }

   bool hasLeft (size_t index) const {
      return left (index) < contents_.length ();
   }

   bool hasRight (size_t index) const {
      return right (index) < contents_.length ();
   }
};

CR_NAMESPACE_END