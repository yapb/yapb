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
#include <crlib/cr-alloc.h>
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
   T *m_data {};
   size_t m_capacity {};
   size_t m_length {};

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
      m_data = rhs.m_data;
      m_length = rhs.m_length;
      m_capacity = rhs.m_capacity;

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
      for (size_t i = 0; i < m_length; ++i) {
         alloc.destruct (&m_data[i]);
      }
   }

   void transferElements (T *dest, T *src, size_t length) noexcept {
      for (size_t i = 0; i < length; ++i) {
         alloc.construct (&dest[i], cr::move (src[i]));
         alloc.destruct (&src[i]);
      }
   }

   void destroy () {
      destructElements ();
      alloc.deallocate (m_data);
   }

   void reset () {
      m_data = nullptr;
      m_capacity = 0;
      m_length = 0;
   }


public:
   bool reserve (const size_t amount) {
      if (m_length + amount < m_capacity) {
         return true;
      }
      auto capacity = m_capacity ? m_capacity : 12;

      if (cr::fix (R == ReservePolicy::Multiple)) {
         while (m_length + amount > capacity) {
            capacity *= 2;
         }
      }
      else {
         capacity = amount + m_capacity + 1;
      }
      auto data = alloc.allocate <T> (capacity);

      if (m_data) {
         transferElements (data, m_data, m_length);
         alloc.deallocate (m_data);
      }

      m_data = data;
      m_capacity = capacity;

      return true;
   }

   bool resize (const size_t amount) {
      if (amount < m_length) {
         while (amount < m_length) {
            discard ();
         }
      }
      else if (amount > m_length) {
         if (!ensure (amount)) {
            return false;
         }
         size_t resizeLength = amount - m_length;

         while (resizeLength--) {
            emplace ();
         }
      }
      return true;
   }

   bool ensure (const size_t amount) {
      if (amount <= m_length) {
         return true;
      }
      return reserve (amount - m_length);
   }

   template <typename U = size_t> U length () const {
      return static_cast <U> (m_length);
   }

   size_t capacity () const {
      return m_capacity;
   }

   template <typename U> bool set (size_t index, U &&object) {
      if (index >= m_capacity) {
         if (!reserve (index + 1)) {
            return false;
         }
      }
      alloc.construct (&m_data[index], cr::forward <U> (object));

      if (index >= m_length) {
         m_length = index + 1;
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
      const size_t capacity = (m_length > index ? m_length : index) + count;

      if (capacity >= m_capacity && !reserve (capacity)) {
         return false;
      }

      if (index >= m_length) {
         for (size_t i = 0; i < count; ++i) {
            alloc.construct (&m_data[i + index], cr::forward <U> (objects[i]));
         }
         m_length = capacity;
      }
      else {
         size_t i = 0;

         for (i = m_length; i > index; --i) {
            m_data[i + count - 1] = cr::move (m_data[i - 1]);
         }
         for (i = 0; i < count; ++i) {
            alloc.construct (&m_data[i + index], cr::forward <U> (objects[i]));
         }
         m_length += count;
      }
      return true;
   }

   bool insert (size_t at, const Array &rhs) {
      if (&rhs == this) {
         return false;
      }
      return insert (at, &rhs.m_data[0], rhs.m_length);
   }

   bool erase (const size_t index, const size_t count) {
      if (index + count > m_capacity) {
         return false;
      }
      for (size_t i = index; i < index + count; ++i) {
         alloc.destruct (&m_data[i]);
      }
      m_length -= count;

      for (size_t i = index; i < m_length; ++i) {
         m_data[i] = cr::move (m_data[i + count]);
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
      alloc.construct (&m_data[m_length], cr::forward <U> (object));
      ++m_length;

      return true;
   }

   template <typename ...Args> bool emplace (Args &&...args) {
      if (!reserve (1)) {
         return false;
      }
      alloc.construct (&m_data[m_length], cr::forward <Args> (args)...);
      ++m_length;

      return true;
   }

   T pop () {
      auto object = cr::move (m_data[m_length - 1]);
      discard ();

      return object;
   }

   void discard () {
      erase (m_length - 1, 1);
   }

   size_t index (const T &object) const {
      return &object - &m_data[0];
   }

   void shuffle () {
      for (size_t i = m_length; i >= 1; --i) {
         cr::swap (m_data[i - 1], m_data[rg.int_ (i, m_length - 2)]);
      }
   }

   void reverse () {
      for (size_t i = 0; i < m_length / 2; ++i) {
         cr::swap (m_data[i], m_data[m_length - 1 - i]);
      }
   }

   template <typename U> bool extend (U &&rhs) {
      if (m_length == 0) {
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
      m_length = 0;
   }

   bool empty () const {
      return m_length == 0;
   }

   bool shrink () {
      if (m_length == m_capacity || !m_length) {
         return false;
      }

      auto data = alloc.allocate <T> (m_length);
      transferElements (data, m_data, m_length);

      alloc.deallocate (m_data);

      m_data = data;
      m_capacity = m_length;

      return true;
   }

   const T &at (size_t index) const {
      return m_data[index];
   }

   T &at (size_t index) {
      return m_data[index];
   }

   const T &first () const {
      return m_data[0];
   }

   T &first () {
      return m_data[0];
   }

   T &last () {
      return m_data[m_length - 1];
   }

   const T &last () const {
      return m_data[m_length - 1];
   }

   const T &random () const {
      return m_data[rg.int_ <size_t> (0, m_length - 1)];
   }

   T &random () {
      return m_data[rg.int_ <size_t> (0u, m_length - 1u)];
   }

   T *data () {
      return m_data;
   }

   T *data () const {
      return m_data;
   }

public:
   Array &operator = (Array &&rhs) noexcept {
      if (this != &rhs) {
         destroy ();

         m_data = rhs.m_data;
         m_length = rhs.m_length;
         m_capacity = rhs.m_capacity;

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
      return m_data;
   }

   T *begin () const {
      return m_data;
   }

   T *end () {
      return m_data + m_length;
   }

   T *end () const {
      return m_data + m_length;
   }
};

// small array (with minimal reserve policy, something like fixed array, but still able to grow, by default allocates 64 elements)
template <typename T> using SmallArray = Array <T, ReservePolicy::Single, 64>;

CR_NAMESPACE_END

