//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
// 

#pragma once

#include <crlib/cr-basic.h>
#include <crlib/cr-array.h>
#include <crlib/cr-string.h>
#include <crlib/cr-twin.h>

CR_NAMESPACE_BEGIN

template <typename T> struct Hash;

template <> struct Hash <String> {
   uint32 operator () (const String &key) const noexcept {
      return key.hash ();
   }
};

template <> struct Hash <StringRef> {
   uint32 operator () (const StringRef &key) const noexcept {
      return key.hash ();
   }
};

template <> struct Hash <const char *> {
   uint32 operator () (const char *key) const noexcept {
      return StringRef::fnv1a32 (key);
   }
};

template <> struct Hash <int32> {
   uint32 operator () (int32 key) const noexcept {
      auto result = static_cast <uint32> (key);

      result = ((result >> 16) ^ result) * 0x119de1f3;
      result = ((result >> 16) ^ result) * 0x119de1f3;
      result = (result >> 16) ^ result;

      return result;
   }
};

template <typename T> struct EmptyHash {
   uint32 operator () (T key) const noexcept {
      return static_cast <uint32> (key);
   }
};

namespace detail {
   template <typename K, typename V> struct HashEntry final : DenyCopying {
   public:
      K key {};
      V value {};
      bool used { false };

   public:
      HashEntry () = default;
      ~HashEntry () = default;

   public:
      HashEntry (HashEntry &&rhs) noexcept : used (rhs.used), key (cr::move (rhs.key)), value (cr::move (rhs.value))
      { }

   public:
      HashEntry &operator = (HashEntry &&rhs) noexcept {
         if (this != &rhs) {
            key = cr::move (rhs.key);
            value = cr::move (rhs.value);
            used = rhs.used;
         }
         return *this;
      }
   };
}

template <typename K, typename V, typename H = Hash <K>> class HashMap final : public DenyCopying {
public:
   using Entries = detail::HashEntry <K, V> [];

private:
   size_t capacity_ {};
   size_t length_ {};
   H hash_;

   UniquePtr <Entries> contents_;

public:
   explicit HashMap (const size_t capacity = 3) : capacity_ (capacity), length_ (0) {
      contents_ = cr::makeUnique <Entries> (capacity);
   }

   HashMap (HashMap &&rhs) noexcept : contents_ (cr::move (rhs.contents_)), hash_ (cr::move (rhs.hash_)), capacity_ (rhs.capacity_), length_ (rhs.length_)
   { }

   ~HashMap () = default;

private:
   size_t getIndex (const K &key, size_t length) const {
      return hash_ (key) % length;
   }

   void rehash (size_t targetCapacity) {
      if (length_ + targetCapacity < capacity_) {
         return;
      }
      auto capacity = capacity_ ? capacity_ : 3;

      while (length_ + targetCapacity > capacity) {
         capacity *= 2;
      }
      auto contents = cr::makeUnique <Entries> (capacity);

      for (size_t i = 0; i < capacity_; ++i) {
         if (contents_[i].used) {
            auto result = put (contents_[i].key, contents, capacity);
            contents[result.second].value = cr::move (contents_[i].value);
         }
      }
      contents_ = cr::move (contents);
      capacity_ = capacity;
   }

   Twin <bool, size_t> put (const K &key, UniquePtr <Entries> &contents, const size_t capacity) {
      size_t index = getIndex (key, capacity);

      for (size_t i = 0; i < capacity; ++i) {
         if (!contents[index].used) {
            contents[index].key = key;
            contents[index].used = true;

            return { true, index };
         }

         if (contents[index].key == key) {
            return { false, index };
         }
         index++;

         if (index == capacity) {
            index = 0;
         }
      }
      return { false, 0 };
   }

public:
   bool empty () const {
      return !length_;
   }

   size_t length () const {
      return length_;
   }

   bool has (const K &key) const  {
      if (empty ()) {
         return false;
      }
      size_t index = getIndex (key, capacity_);

      for (size_t i = 0; i < capacity_; ++i) {
         if (contents_[index].used && contents_[index].key == key) {
            return true;
         }
         if (++index == capacity_) {
            break;
         }
      }
      return false;
   }

   void erase (const K &key) {
      size_t index = getIndex (key, capacity_);

      for (size_t i = 0; i < capacity_; ++i) {
         if (contents_[index].used && contents_[index].key == key) {
            contents_[index].used = false;
            --length_;

            break;
         }
         if (++index == capacity_) {
            break;
         }
      }
   }

   void clear () {
      length_ = 0;

      for (size_t i = 0; i < capacity_; ++i) {
         contents_[i].used = false;
      }
      rehash (0);
   }

   void foreach (Lambda <void (const K &, const V &)> callback) {
      for (size_t i = 0; i < capacity_; ++i) {
         if (contents_[i].used) {
            callback (contents_[i].key, contents_[i].value);
         }
      }
   }

public:
   V &operator [] (const K &key) {
      if (length_ >= capacity_) {
         rehash (length_ << 1);
      }
      auto result = put (key, contents_, capacity_);

      if (result.first) {
         ++length_;
      }
      return contents_[result.second].value;
   }

   HashMap &operator = (HashMap &&rhs) noexcept {
      if (this != &rhs) {
         contents_ = cr::move (rhs.contents_);
         hash_ = cr::move (rhs.hash_);

         length_ = rhs.length_;
         capacity_ = rhs.capacity_;
      }
      return *this;
   }
};

CR_NAMESPACE_END
