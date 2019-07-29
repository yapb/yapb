//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <string.h>
#include <ctype.h>

#include <crlib/cr-basic.h>
#include <crlib/cr-alloc.h>
#include <crlib/cr-movable.h>
#include <crlib/cr-twin.h>
#include <crlib/cr-uniqueptr.h>

CR_NAMESPACE_BEGIN

// small-string optimized string class, sso stuff based on: https://github.com/elliotgoodrich/SSO-23/
class String final  {
public:
   static constexpr size_t kInvalidIndex = static_cast <size_t> (-1);

private:
   static constexpr size_t kExcessSpace = 32;

private:
   using Length = Twin <size_t, size_t>;

private:
   union Data {
      struct Big {
         char excess[kExcessSpace - sizeof (char *) - 2 * sizeof (size_t)];
         char *ptr;
         size_t length;
         size_t capacity;
      } big;

      struct Small {
         char str[sizeof (Big) / sizeof (char) - 1];
         uint8 length;
      } small;
   } m_data;

private:
   static size_t const kSmallCapacity = sizeof (typename Data::Big) / sizeof (char) - 1;

public:
   explicit String () {
      reset ();
      assign ("", 0);
   }

   String (const char *str, size_t length = 0) {
      reset ();
      assign (str, length);
   }

   String (const String &str) {
      reset ();
      assign (str.data (), str.length ());
   }

   String (const char ch) {
      reset ();
      assign (ch);
   }

   String (String &&rhs) noexcept {
      m_data = rhs.m_data;
      rhs.setMoved ();
   }

   ~String () {
      destroy ();
   }

private:
   template <typename T> static uint8 &getMostSignificantByte (T &object) {
      return *(reinterpret_cast <uint8 *> (&object) + sizeof (object) - 1);
   }

   template <size_t N> static bool getLeastSignificantBit (uint8 byte) {
      return byte & cr::bit (N);
   }

   template <size_t N> static bool getMostSignificantBit (uint8 byte) {
      return byte & cr::bit (CHAR_BIT - N - 1);
   }

   template <size_t N> static void setLeastSignificantBit (uint8 &byte, bool bit) {
      if (bit) {
         byte |= cr::bit (N);
      }
      else {
         byte &= ~cr::bit (N);
      }
   }

   template <size_t N> static void setMostSignificantBit (uint8 &byte, bool bit) {
      if (bit) {
         byte |= cr::bit (CHAR_BIT - N - 1);
      }
      else {
         byte &= ~cr::bit (CHAR_BIT - N - 1);
      }
   }

   void destroy () {
      if (!isSmall ()) {
         alloc.deallocate (m_data.big.ptr);
      }
   }

   void reset () {
      m_data.small.length = 0;
      m_data.small.str[0] = '\0';

      m_data.big.ptr = nullptr;
      m_data.big.length = 0;
   }

   void endString (const char *str, size_t at) {
      const_cast <char *> (str)[at] = '\0';
   }

   void moveString (const char *dst, const char *src, size_t length) {
      if (!dst) {
         return;
      }
      memmove (const_cast <char *> (dst), src, length);
   }

   const char *data () const {
      return isSmall () ? m_data.small.str : m_data.big.ptr;
   }

   void setMoved () {
      setSmallLength (0);
   }

   void setLength (size_t amount, size_t capacity) {
      if (amount <= kSmallCapacity) {
         endString (m_data.small.str, amount);
         setSmallLength (static_cast <uint8> (amount));
      }
      else {
         endString (m_data.big.ptr, amount);
         setDataNonSmall (amount, capacity);
      }
   }

   bool isSmall () const  {
      return !getLeastSignificantBit <0> (m_data.small.length) && !getLeastSignificantBit <1> (m_data.small.length);
   }

   void setSmallLength (uint8 length) {
      m_data.small.length = static_cast <char> (kSmallCapacity - length) << 2;
   }

   size_t getSmallLength () const {
      return kSmallCapacity - ((m_data.small.length >> 2) & 63u);
   }

   void setDataNonSmall (size_t length, size_t capacity) {
      uint8 &lengthHighByte = getMostSignificantByte (length);
      uint8 &capacityHighByte = getMostSignificantByte (capacity);

      const bool lengthHasHighBit = getMostSignificantBit <0> (lengthHighByte);
      const bool capacityHasHighBit = getMostSignificantBit <0> (capacityHighByte);
      const bool capacityHasSecHighBit = getMostSignificantBit <1> (capacityHighByte);

      setMostSignificantBit <0> (lengthHighByte, capacityHasSecHighBit);

      capacityHighByte <<= 2;
      setLeastSignificantBit <0> (capacityHighByte, capacityHasHighBit);
      setLeastSignificantBit <1> (capacityHighByte, !lengthHasHighBit);

      m_data.big.length = length;
      m_data.big.capacity = capacity;
   }

   Length getDataNonSmall () const {
      size_t length = m_data.big.length;
      size_t capacity = m_data.big.capacity;

      uint8 &lengthHighByte = getMostSignificantByte (length);
      uint8 &capacityHighByte = getMostSignificantByte (capacity);

      const bool capacityHasHighBit = getLeastSignificantBit <0> (capacityHighByte);
      const bool lengthHasHighBit = !getLeastSignificantBit <1> (capacityHighByte);
      const bool capacityHasSecHighBit = getMostSignificantBit <0> (lengthHighByte);

      setMostSignificantBit <0> (lengthHighByte, lengthHasHighBit);

      capacityHighByte >>= 2;
      setMostSignificantBit <0> (capacityHighByte, capacityHasHighBit);
      setMostSignificantBit <1> (capacityHighByte, capacityHasSecHighBit);

      return { length, capacity };
   }

public:
   String &assign (const char *str, size_t length = 0) {
      length = length > 0 ? length : strlen (str);

      if (length <= kSmallCapacity) {
         moveString (m_data.small.str, str, length);

         endString (m_data.small.str, length);
         setSmallLength (static_cast <uint8> (length));
      }
      else {
         auto capacity = cr::max (kSmallCapacity * 2, length);
         m_data.big.ptr = alloc.allocate <char> (capacity + 1);

         if (m_data.big.ptr) {
            moveString (m_data.big.ptr, str, length);

            endString (m_data.big.ptr, length);
            setDataNonSmall (length, capacity);
         }
      }
      return *this;
   }

   String &assign (const String &str, size_t length = 0) {
      return assign (str.chars (), length);
   }

   String &assign (const char ch) {
      const char str[] { ch, '\0' };
      return assign (str, strlen (str));
   }

   String &append (const char *str, size_t length = 0) {
      if (empty ()) {
         return assign (str, length);
      }
      length = length > 0 ? length : strlen (str);

      size_t oldLength = this->length ();
      size_t newLength = oldLength + length;

      resize (newLength);

      moveString (&data ()[oldLength], str, length);
      endString (data (), newLength);

      return *this;
   }

   String &append (const String &str, size_t length = 0) {
      return append (str.chars (), length);
   }

   String &append (const char ch) {
      const char str[] { ch, '\0' };
      return append (str, strlen (str));
   }

   template <typename ...Args> String &assignf (const char *fmt, Args ...args) {
      const size_t size = snprintf (nullptr, 0, fmt, args...);

      SmallArray <char> buffer (size + 1);
      snprintf (buffer.data (), size + 1, fmt, cr::forward <Args> (args)...);

      return assign (buffer.data ());
   }

   template <typename ...Args> String &appendf (const char *fmt, Args ...args) {
      if (empty ()) {
         return assignf (fmt, cr::forward <Args> (args)...);
      }
      const size_t size = snprintf (nullptr, 0, fmt, args...) + length ();

      SmallArray <char> buffer (size + 1);
      snprintf (buffer.data (), size + 1, fmt, cr::forward <Args> (args)...);

      return append (buffer.data ());
   }

   void resize (size_t amount) {
      size_t oldLength = length ();

      if (amount <= kSmallCapacity) {
         if (!isSmall ()) {
            auto ptr = m_data.big.ptr;

            moveString (m_data.small.str, ptr, cr::min (oldLength, amount));
            alloc.deallocate (ptr);
         }
         setLength (amount, 0);
      }
      else {
         size_t newCapacity = 0;

         if (isSmall ()) {
            newCapacity = cr::max (amount, kSmallCapacity * 2);
            auto ptr = alloc.allocate <char> (newCapacity + 1);

            moveString (ptr, m_data.small.str, cr::min (oldLength, amount));
            m_data.big.ptr = ptr;
         }
         else if (amount < capacity ()) {
            newCapacity = capacity ();
         }
         else {
            newCapacity = cr::max (amount, capacity () * 3 / 2);
            auto ptr = alloc.allocate <char> (newCapacity + 1);

            moveString (ptr, m_data.big.ptr, cr::min (oldLength, amount));
            alloc.deallocate (m_data.big.ptr);

            m_data.big.ptr = ptr;
         }
         setLength (amount, newCapacity);
      }
   }

   bool insert (size_t index, const String &str) {
      if (str.empty ()) {
         return false;
      }

      const size_t strLength = str.length ();
      const size_t dataLength = length ();

      if (index >= dataLength) {
         append (str.chars (), strLength);
      }
      else {
         resize (dataLength + strLength);

         for (size_t i = dataLength; i > index; --i) {
            at (i + strLength - 1) = at (i - 1);
         }

         for (size_t i = 0; i < strLength; ++i) {
            at (i + index) = str.at (i);
         }
      }
      return true;
   }

   bool erase (size_t index, size_t count = 1) {
      const size_t dataLength = length ();

      if (index + count > dataLength) {
         return false;
      }
      const size_t newLength = dataLength - count;

      for (size_t i = index; i < newLength; ++i) {
         at (i) = at (i + count);
      }
      resize (newLength);

      return true;
   }

   size_t find (char pattern, size_t start = 0) const {
      for (size_t i = start; i < length (); ++i) {
         if (at (i) == pattern) {
            return i;
         }
      }
      return kInvalidIndex;
   }

   size_t find (const String &pattern, size_t start = 0) const {
      const size_t patternLength = pattern.length ();
      const size_t dataLength = length ();

      if (patternLength > dataLength || start > dataLength) {
         return kInvalidIndex;
      }

      for (size_t i = start; i <= dataLength - patternLength; ++i) {
         size_t index = 0;

         for (; at (index) && index < patternLength; ++index) {
            if (at (i + index) != pattern.at (index)) {
               break;
            }
         }

         if (!pattern.at (index)) {
            return i;
         }
      }
      return kInvalidIndex;
   }

   size_t rfind (char pattern) const {
      for (size_t i = length (); i != 0; i--) {
         if (at (i) == pattern) {
            return i;
         }
      }
      return kInvalidIndex;
   }

   size_t rfind (const String &pattern) const {
      const size_t patternLength = pattern.length ();
      const size_t dataLength = length ();

      if (patternLength > dataLength) {
         return kInvalidIndex;
      }
      bool match = true;

      for (size_t i = dataLength - 1; i >= patternLength; i--) {
         match = true;

         for (size_t j = patternLength - 1; j > 0; j--) {
            if (at (i + j) != pattern.at (j)) {
               match = false;
               break;
            }
         }

         if (match) {
            return i;
         }
      }
      return kInvalidIndex;
   }

   size_t findFirstOf (const String &pattern, size_t start = 0) const {
      const size_t patternLength = pattern.length ();
      const size_t dataLength = length ();

      for (size_t i = start; i < dataLength; ++i) {
         for (size_t j = 0; j < patternLength; ++j) {
            if (at (i) == pattern.at (j)) {
               return i;
            }
         }
      }
      return kInvalidIndex;
   }

   size_t findLastOf (const String &pattern) const {
      const size_t patternLength = pattern.length ();
      const size_t dataLength = length ();

      for (size_t i = dataLength - 1; i > 0; i--) {
         for (size_t j = 0; j < patternLength; ++j) {
            if (at (i) == pattern.at (j)) {
               return i;
            }
         }
      }
      return kInvalidIndex;
   }

   size_t findFirstNotOf (const String &pattern, size_t start = 0) const {
      const size_t patternLength = pattern.length ();
      const size_t dataLength = length ();

      bool different = true;

      for (size_t i = start; i < dataLength; ++i) {
         different = true;

         for (size_t j = 0; j < patternLength; ++j) {
            if (at (i) == pattern.at (j)) {
               different = false;
               break;
            }
         }

         if (different) {
            return i;
         }
      }
      return kInvalidIndex;
   }

   size_t findLastNotOf (const String &pattern) const {
      const size_t patternLength = pattern.length ();
      const size_t dataLength = length ();

      bool different = true;

      for (size_t i = dataLength - 1; i > 0; i--) {
         different = true;

         for (size_t j = 0; j < patternLength; ++j) {
            if (at (i) == pattern.at (j)) {
               different = false;
               break;
            }
         }
         if (different) {
            return i;
         }
      }
      return kInvalidIndex;
   }


   size_t countChar (char ch) const {
      size_t count = 0;

      for (size_t i = 0, e = length (); i != e; ++i) {
         if (at (i) == ch) {
            ++count;
         }
      }
      return count;
   }

   size_t countStr (const String &pattern) const {
      const size_t patternLen = pattern.length ();
      const size_t dataLength = length ();

      if (patternLen > dataLength) {
         return 0;
      }
      size_t count = 0;

      for (size_t i = 0, e = dataLength - patternLen + 1; i != e; ++i) {
         if (substr (i, patternLen).compare (pattern)) {
            ++count;
         }
      }
      return count;
   }

   String substr (size_t start, size_t count = kInvalidIndex) const {
      start = cr::min (start, length ());

      if (count == kInvalidIndex) {
         count = length ();
      }
      return String (data () + start, cr::min (count, length () - start));
   }

   size_t replace (const String &needle, const String &to) {
      if (needle.empty () || to.empty ()) {
         return 0;
      }
      size_t replaced = 0, pos = 0;

      while (pos < length ()) {
         pos = find (needle, pos);

         if (pos == kInvalidIndex) {
            break;
         }
         erase (pos, needle.length ());
         insert (pos, to);

         pos += to.length ();
         replaced++;
      }
      return replaced;
   }

   bool startsWith (const String &prefix) const {
      const size_t prefixLength = prefix.length ();
      const size_t dataLength = length ();

      return prefixLength <= dataLength && strncmp (data (), prefix.data (), prefixLength) == 0;
   }

   bool endsWith (const String &suffix) const {
      const size_t suffixLength = suffix.length ();
      const size_t dataLength = length ();

      return suffixLength <= dataLength && strncmp (data () + dataLength - suffixLength, suffix.data (), suffixLength) == 0;
   }

   Array <String> split (const String &delim) const {
      Array <String> tokens;
      size_t prev = 0, pos = 0;

      while ((pos = find (delim, pos)) != kInvalidIndex) {
         tokens.push (substr (prev, pos - prev));
         prev = ++pos;
      }
      tokens.push (substr (prev, pos - prev));

      return tokens;
   }

   size_t length () const {
      if (isSmall ()) {
         return getSmallLength ();
      }
      else {
         return getDataNonSmall ().first;
      }
   }

   size_t capacity () const {
      if (isSmall ()) {
         return sizeof (m_data) - 1;
      }
      else {
         return getDataNonSmall ().second;
      }
   }

   bool small () const {
      return isSmall ();
   }

   bool empty () const {
      return length () == 0;
   }

   void clear () {
      assign ("");
   }

   const char *chars () const {
      return data ();
   }

   const char &at (size_t index) const {
      return begin ()[index];
   }

   char &at (size_t index) {
      return begin ()[index];
   }

   int32 compare (const String &rhs) const {
      return strcmp (rhs.data (), data ());
   }

   int32 compare (const char *rhs) const {
      return strcmp (rhs, data ());
   }

   bool contains (const String &rhs) const {
      return find (rhs) != kInvalidIndex;
   }

   String &lowercase () {
      for (auto &ch : *this) {
         ch = static_cast <char> (::tolower (ch));
      }
      return *this;
   }

   String &uppercase () {
      for (auto &ch : *this) {
         ch = static_cast <char> (::toupper (ch));
      }
      return *this;
   }

   int32 int_ () const {
      return atoi (data ());
   }

   float float_ () const {
      return static_cast <float> (atof (data ()));
   }

   String &ltrim (const String &characters = "\r\n\t ") {
      size_t begin = length ();

      for (size_t i = 0; i < begin; ++i) {
         if (characters.find (at (i)) == kInvalidIndex) {
            begin = i;
            break;
         }
      }
      return *this = substr (begin, length () - begin);
   }

   String &rtrim (const String &characters = "\r\n\t ") {
      size_t end = 0;

      for (size_t i = length (); i > 0; --i) {
         if (characters.find (at (i - 1)) == kInvalidIndex) {
            end = i;
            break;
         }
      }
      return *this = substr (0, end);
   }

   String &trim (const String &characters = "\r\n\t ") {
      return ltrim (characters).rtrim (characters);
   }

public:
   String &operator = (String &&rhs) noexcept {
      destroy ();

      m_data = rhs.m_data;
      rhs.setMoved ();

      return *this;
   }

   String &operator = (const String &rhs) {
      return assign (rhs);
   }

   String &operator = (const char *rhs) {
      return assign (rhs);
   }

   String &operator = (char rhs) {
      return assign (rhs);
   }

   String &operator += (const String &rhs) {
      return append (rhs);
   }

   String &operator += (const char *rhs) {
      return append (rhs);
   }

   const char &operator [] (size_t index) const {
      return at (index);
   }

   char &operator [] (size_t index) {
      return at (index);
   }

   friend String operator + (const String &lhs, char rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (char lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (const String &lhs, const char *rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (const char *lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend String operator + (const String &lhs, const String &rhs) {
      return String (lhs).append (rhs);
   }

   friend bool operator == (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) == 0;
   }

   friend bool operator < (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) < 0;
   }

   friend bool operator > (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) > 0;
   }

   friend bool operator == (const char *lhs, const String &rhs) {
      return rhs.compare (lhs) == 0;
   }

   friend bool operator == (const String &lhs, const char *rhs) {
      return lhs.compare (rhs) == 0;
   }

   friend bool operator != (const String &lhs, const String &rhs) {
      return lhs.compare (rhs) != 0;
   }

   friend bool operator != (const char *lhs, const String &rhs) {
      return rhs.compare (lhs) != 0;
   }

   friend bool operator != (const String &lhs, const char *rhs) {
      return lhs.compare (rhs) != 0;
   }

public:
   static String join (const Array <String> &sequence, const String &delim, size_t start = 0) {
      if (sequence.empty ()) {
         return "";
      }

      if (sequence.length () == 1) {
         return sequence.at (0);
      }
      String result;

      for (size_t index = start; index < sequence.length (); ++index) {
         if (index != start) {
            result += delim + sequence[index];
         }
         else {
            result += sequence[index];
         }
      }
      return result;
   }

   // for range-based loops
public:
   char *begin () {
      return const_cast <char *> (data ());
   }

   char *begin () const {
      return const_cast <char *> (data ());
   }

   char *end () {
      return begin () + length ();
   }

   char *end () const {
      return begin () + length ();
   }
};

// simple rotation-string pool for holding temporary data passed to different modules and for formatting
class StringBuffer final : public Singleton <StringBuffer> {
public:
   enum : size_t {
      StaticBufferSize = static_cast <size_t> (768),
      RotationCount = static_cast <size_t> (32)
   };

private:
   char m_data[RotationCount + 1][StaticBufferSize] {};
   size_t m_rotate = 0;

public:
   StringBuffer () = default;
   ~StringBuffer () = default;

public:
   char *chars () {
      if (++m_rotate >= RotationCount) {
         m_rotate = 0;
      }
      return m_data[cr::clamp <size_t> (m_rotate, 0, RotationCount)];
   }

   template <typename U, typename ...Args> U *format (const U *fmt, Args ...args) {
      auto buffer = Singleton <StringBuffer>::get ().chars ();
      snprintf (buffer, StaticBufferSize, fmt, args...);

      return buffer;
   }

   template <typename U> U *format (const U *fmt) {
      auto buffer = Singleton <StringBuffer>::get ().chars ();
      strncpy (buffer, fmt, StaticBufferSize);

      return buffer;
   }
};

// expose global string pool
static auto &strings = StringBuffer::get ();

CR_NAMESPACE_END