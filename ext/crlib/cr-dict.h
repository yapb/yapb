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
#include <crlib/cr-array.h>
#include <crlib/cr-string.h>
#include <crlib/cr-twin.h>

CR_NAMESPACE_BEGIN

// template for hashing our string
template <typename K> struct StringHash {
   uint32 operator () (const K &key) const {
      auto str = const_cast <char *> (key.chars ());
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
      uint32 hash = static_cast <uint32> (-1);
      K key {};
      V value {};

   public:
      DictionaryBucket () = default;
      ~DictionaryBucket () = default;

   public:
      DictionaryBucket (DictionaryBucket &&rhs) noexcept : hash (rhs.hash), key (cr::move (rhs.key)), value (cr::move (rhs.value))
      { }

   public:
      DictionaryBucket &operator = (DictionaryBucket &&rhs) noexcept {
         if (this != &rhs) {
            key = cr::move (rhs.key);
            value = cr::move (rhs.value);
            hash = rhs.hash;
         }
         return *this;
      }
   };
}

// basic dictionary
template <class K, class V, class H = StringHash <K>, size_t HashSize = 36> class Dictionary final : public DenyCopying {
private:
   using DictBucket = detail::DictionaryBucket <K, V>;
   using DictList = detail::DictionaryList;

public:
   enum : size_t {
      InvalidIndex = static_cast <size_t> (-1)
   };

private:
   Array <DictList *> contents_;
   Array <DictBucket> buckets_;
   H hashFunction_;

private:
   uint32 hash (const K &key) const {
      return hashFunction_ (key);
   }

   size_t find (const K &key, bool allocate) {
      auto hashed = hash (key);
      auto pos = hashed % contents_.length ();

      for (auto bucket = contents_[pos]; bucket != nullptr; bucket = bucket->next) {
         if (buckets_[bucket->index].hash == hashed) {
            return bucket->index;
         }
      }

      if (allocate) {
         size_t created = buckets_.length ();
         buckets_.resize (created + 1);

         auto allocated = alloc.allocate <DictList> ();

         allocated->index = static_cast <int32> (created);
         allocated->next = contents_[pos];

         contents_[pos] = allocated;

         buckets_[created].key = key;
         buckets_[created].hash = hashed;

         return created;
      }
      return InvalidIndex;
   }

   size_t findIndex (const K &key) const {
      return const_cast <Dictionary *> (this)->find (key, false);
   }

public:
   explicit Dictionary () {
      reset ();
   }

   Dictionary (Dictionary &&rhs) noexcept : contents_ (cr::move (rhs.contents_)), buckets_ (cr::move (rhs.buckets_)), hashFunction_ (cr::move (rhs.hashFunction_))
   { }

   ~Dictionary () {
      clear ();
   }

public:
   bool exists (const K &key) const {
      return findIndex (key) != InvalidIndex;
   }

   bool empty () const {
      return buckets_.empty ();
   }

   size_t length () const {
      return buckets_.length ();
   }

   bool find (const K &key, V &value) const {
      size_t index = findIndex (key);

      if (index == InvalidIndex) {
         return false;
      }
      value = buckets_[index].value;
      return true;
   }

   template <typename U> bool push (const K &key, U &&value) {
      operator [] (key) = cr::forward <U> (value);
      return true;
   }

   bool remove (const K &key) {
      auto hashed = hash (key);

      auto pos = hashed % contents_.length ();
      auto *bucket = contents_[pos];

      DictList *next = nullptr;

      while (bucket != nullptr) {
         if (buckets_[bucket->index].hash == hashed) {
            if (!next) {
               contents_[pos] = bucket->next;
            }
            else {
               next->next = bucket->next;
            }
            buckets_.erase (bucket->index, 1);

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
      for (auto object : contents_) {
         while (object != nullptr) {
            auto next = object->next;

            alloc.deallocate (object);
            object = next;
         }
      }
      contents_.clear ();
      buckets_.clear ();

      reset ();
   }

   void reset () {
      contents_.resize (HashSize);

      for (size_t i = 0; i < HashSize; ++i) {
         contents_[i] = nullptr;
      }
   }

public:
   V &operator [] (const K &key) {
      return buckets_[find (key, true)].value;
   }

   const V &operator [] (const K &key) const {
      return buckets_[findIndex (key)].value;
   }

   Dictionary &operator = (Dictionary &&rhs) noexcept {
      if (this != &rhs) {
         contents_ = cr::move (rhs.contents_);
         buckets_ = cr::move (rhs.buckets_);
         hashFunction_ = cr::move (rhs.hashFunction_);
      }
      return *this;
   }

   // for range-based loops
public:
   DictBucket *begin () {
      return buckets_.begin ();
   }

   DictBucket *begin () const {
      return buckets_.begin ();
   }

   DictBucket *end () {
      return buckets_.end ();
   }

   DictBucket *end () const {
      return buckets_.end ();
   }
};

CR_NAMESPACE_END
