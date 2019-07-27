//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
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
         size_t parent = this->parent (index);

         if (m_data[parent] <= m_data[index]) {
            break;
         }
         cr::swap (m_data[index], m_data[parent]);
         index = parent;
      }
   }

   void percolateDown (size_t index) {
      while (this->left (index) < m_data.length ()) {
         size_t best = this->left (index);

         if (this->right (index) < m_data.length ()) {
            size_t right_index = this->right (index);

            if (m_data[right_index] < m_data[best]) {
               best = right_index;
            }
         }

         if (m_data[index] <= m_data[best]) {
            break;
         }
         cr::swap (m_data[index], m_data[best]);

         index = best;
         best = this->left (index);
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
   static constexpr size_t left (size_t index) {
      return index << 1 | 1;
   }

   static constexpr size_t right (size_t index) {
      return ++index << 1;
   }

   static constexpr size_t parent (size_t index) {
      return --index >> 1;
   }
};

CR_NAMESPACE_END