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
#include <crlib/cr-array.h>
#include <crlib/cr-string.h>
#include <crlib/cr-twin.h>

CR_NAMESPACE_BEGIN

// template for hashing our string
template <typename K> struct StringHash {
   uint32 operator () (const K &key) const {
      char *str = const_cast <char *> (key.chars ());
      uint32 hash = 0;

      while (*str++) {
         hash = ((hash << 5) + hash) + *str;
      }
      return hash;
   }
};

// template for hashing integers
template <typename K> struct IntHash {
   uint32 operator () (K key) const {
      key = ((key >> 16) ^ key) * 0x119de1f3;
      key = ((key >> 16) ^ key) * 0x119de1f3;
      key = (key >> 16) ^ key;

      return key;
   }
};

// template for np hashing integers
template <typename K> struct IntNoHash {
   uint32 operator () (K key) const {
      return static_cast <uint32> (key);
   }
};

namespace detail {
   struct DictionaryList {
      uint32 index;
      DictionaryList *next;
   };

   template <typename K, typename V> struct DictionaryBucket {
      uint32 hash = static_cast <size_t> (-1);
      K key = K ();
      V value = V ();

   public:
      DictionaryBucket () = default;
      ~DictionaryBucket () = default;
   };
}

// basic dictionary
template <class K, class V, class H = StringHash <K>, size_t HashSize = 36> class Dictionary final : public DenyCopying {
public:
   enum : size_t {
      InvalidIndex = static_cast <size_t> (-1)
   };

private:
   Array <detail::DictionaryList *> m_table;
   Array <detail::DictionaryBucket <K, V>> m_buckets;
   H m_hasher;

private:
   uint32 hash (const K &key) const {
      return m_hasher (key);
   }

   size_t findIndexOrAllocate (const K &key, bool allocate) {
      auto hashed = hash (key);
      auto pos = hashed % m_table.length ();

      for (auto bucket = m_table[pos]; bucket != nullptr; bucket = bucket->next) {
         if (m_buckets[bucket->index].hash == hashed) {
            return bucket->index;
         }
      }

      if (allocate) {
         size_t created = m_buckets.length ();
         m_buckets.resize (created + 1);

         auto allocated = alloc.allocate <detail::DictionaryList> ();

         allocated->index = created;
         allocated->next = m_table[pos];

         m_table[pos] = allocated;

         m_buckets[created].key = key;
         m_buckets[created].hash = hashed;

         return created;
      }
      return InvalidIndex;
   }

   size_t findIndex (const K &key) const {
      return const_cast <Dictionary *> (this)->findIndexOrAllocate (key, false);
   }

public:
   explicit Dictionary () {
      reset ();
   }

   Dictionary (Dictionary &&rhs) noexcept : m_table (cr::move (rhs.m_table)), m_buckets (cr::move (rhs.m_buckets)), m_hasher (cr::move (rhs.m_hasher))
   { }

   ~Dictionary () {
      clear ();
   }

public:
   bool exists (const K &key) const {
      return findIndex (key) != InvalidIndex;
   }

   bool empty () const {
      return m_buckets.empty ();
   }

   size_t length () const {
      return m_buckets.length ();
   }

   bool find (const K &key, V &value) const {
      size_t index = findIndex (key);

      if (index == InvalidIndex) {
         return false;
      }
      value = m_buckets[index].value;
      return true;
   }

   template <typename U> bool push (const K &key, U &&value) {
      operator [] (key) = cr::forward <U> (value);
      return true;
   }

   bool remove (const K &key) {
      auto hashed = hash (key);

      auto pos = hashed % m_table.length ();
      auto *bucket = m_table[pos];

      detail::DictionaryList *next = nullptr;

      while (bucket != nullptr) {
         if (m_buckets[bucket->index].hash == hashed) {
            if (!next) {
               m_table[pos] = bucket->next;
            }
            else {
               next->next = bucket->next;
            }
            m_buckets.erase (bucket->index, 1);

            alloc.deallocate (bucket);
            bucket = nullptr;

            return true;
         }
         next = bucket;
         bucket = bucket->next;
      }
      return false;
   }

   void clear () {
      for (auto object : m_table) {
         while (object != nullptr) {
            auto next = object->next;

            alloc.deallocate (object);
            object = next;
         }
      }
      m_table.clear ();
      m_buckets.clear ();

      reset ();
   }

   void reset () {
      m_table.resize (HashSize);

      for (size_t i = 0; i < HashSize; ++i) {
         m_table[i] = nullptr;
      }
   }

public:
   V &operator [] (const K &key) {
      return m_buckets[findIndexOrAllocate (key, true)].value;
   }

   const V &operator [] (const K &key) const {
      return m_buckets[findIndex (key)].value;
   }

   Dictionary &operator = (Dictionary &&rhs) noexcept {
      if (this != &rhs) {
         m_table = cr::move (rhs.m_table);
         m_buckets = cr::move (rhs.m_buckets);
         m_hasher = cr::move (rhs.m_hasher);
      }
      return *this;
   }

   // for range-based loops
public:
   detail::DictionaryBucket <K, V> *begin () {
      return m_buckets.begin ();
   }

   detail::DictionaryBucket <K, V> *begin () const {
      return m_buckets.begin ();
   }

   detail::DictionaryBucket <K, V> *end () {
      return m_buckets.end ();
   }

   detail::DictionaryBucket <K, V> *end () const {
      return m_buckets.end ();
   }
};

CR_NAMESPACE_END