//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
//

#pragma once

#include <crlib/cr-array.h>

CR_NAMESPACE_BEGIN

// simple priority queue
template <typename T> class BinaryHeap final : public DenyCopying {
private:
   Array <T> m_data;

public:
   explicit BinaryHeap () = default;

   BinaryHeap (BinaryHeap &&rhs) noexcept : m_data (cr::move (rhs.m_data))
   { }

   ~BinaryHeap () = default;

public:
   template <typename U> bool push (U &&item) {
      if (!m_data.push (cr::move (item))) {
         return false;
      }
      size_t length = m_data.length ();

      if (length > 1) {
         percolateUp (length - 1);
      }
      return true;
   }

   template <typename ...Args> bool emplace (Args &&...args) {
      if (!m_data.emplace (cr::forward <Args> (args)...)) {
         return false;
      }
      size_t length = m_data.length ();

      if (length > 1) {
         percolateUp (length - 1);
      }
      return true;
   }

   const T &top () const {
      return m_data[0];
   }

   T pop () {
      if (m_data.length () == 1) {
         return m_data.pop ();
      }
      auto key (cr::move (m_data[0]));

      m_data[0] = cr::move (m_data.last ());
      m_data.discard ();

      percolateDown (0);
      return key;
   }

public:
   size_t length () const {
      return m_data.length ();
   }

   bool empty () const {
      return !m_data.length ();
   }

   void clear () {
      m_data.clear ();
   }

private:
   void percolateUp (size_t index) {
      while (index != 0) {
         size_t parentIndex = parent (index);

         if (m_data[parentIndex] > m_data[index]) {
            cr::swap (m_data[index], m_data[parentIndex]);
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
            size_t rightIndex = right (index);

            if (m_data[rightIndex] < m_data[bestIndex]) {
               bestIndex = rightIndex;
            }
         }

         if (m_data[index] > m_data[bestIndex]) {
            cr::swap (m_data[index], m_data[bestIndex]);

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
         m_data = cr::move (rhs.m_data);
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
      return left (index) < m_data.length ();
   }

   bool hasRight (size_t index) const {
      return right (index) < m_data.length ();
   }
};

CR_NAMESPACE_END