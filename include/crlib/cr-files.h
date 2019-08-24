//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <stdio.h>

#include <crlib/cr-string.h>
#include <crlib/cr-lambda.h>

CR_NAMESPACE_BEGIN

// simple stdio file wrapper
class File final : private DenyCopying {
private:
   FILE *m_handle = nullptr;
   size_t m_length = 0;

public:
   explicit File () = default;
   
   File (const String &file, const String &mode = "rt") {
      open (file, mode);
   }

   ~File () {
      close ();
   }
public:

   bool open (const String &file, const String &mode) {
      if ((m_handle = fopen (file.chars (), mode.chars ())) == nullptr) {
         return false;
      }
      fseek (m_handle, 0L, SEEK_END);
      m_length = ftell (m_handle);
      fseek (m_handle, 0L, SEEK_SET);

      return true;
   }

   void close () {
      if (m_handle != nullptr) {
         fclose (m_handle);
         m_handle = nullptr;
      }
      m_length = 0;
   }

   bool eof () const {
      return !!feof (m_handle);
   }

   bool flush () const {
      return !!fflush (m_handle);
   }

   int getChar () const {
      return fgetc (m_handle);
   }

   char *getString (char *buffer, int count) const {
      return fgets (buffer, count, m_handle);
   }

   bool getLine (String &line) {
      line.clear ();
      int ch;

      while ((ch = getChar ()) != EOF && !eof ()) {
         line += static_cast <char> (ch);

         if (ch == '\n') {
            break;
         }
      }
      return !eof ();
   }

   template <typename ...Args> size_t puts (const char *fmt, Args ...args) {
      if (!*this) {
         return 0;
      }
      return fprintf (m_handle, fmt, cr::forward <Args> (args)...);
   }

   bool puts (const char *buffer) {
      if (!*this) {
         return 0;
      }
      if (fputs (buffer, m_handle) < 0) {
         return false;
      }
      return true;
   }

   int putChar (int ch) {
      return fputc (ch, m_handle);
   }

   size_t read (void *buffer, size_t size, size_t count = 1) {
      return fread (buffer, size, count, m_handle);
   }

   size_t write (void *buffer, size_t size, size_t count = 1) {
      return fwrite (buffer, size, count, m_handle);
   }

   bool seek (long offset, int origin) {
      if (fseek (m_handle, offset, origin) != 0) {
         return false;
      }
      return true;
   }

   void rewind () {
      ::rewind (m_handle);
   }

   size_t length () const {
      return m_length;
   }

public:
   explicit operator bool () const {
      return m_handle != nullptr;
   }

public:
   static inline bool exists (const String &file) {
      File fp;

      if (fp.open (file, "rb")) {
         fp.close ();
         return true;
      }
      return false;
   }

   static inline void createPath (const char *path) {
      for (auto str = const_cast <char *> (path) + 1; *str; ++str) {
         if (*str == '/') {
            *str = 0;
            plat.createDirectory (path);
            *str = '/';
         }
      }
      plat.createDirectory (path);
   }
};

// wrapper for memory file for loading data into the memory
class MemFileStorage : public Singleton <MemFileStorage> {
private:
   using LoadFunction = Lambda <uint8 * (const char *, int *)>;
   using FreeFunction = Lambda <void (void *)>;

private:
   LoadFunction m_load = nullptr;
   FreeFunction m_free = nullptr;

public:
   inline MemFileStorage () = default;
   inline ~MemFileStorage () = default;

public:
   void initizalize (LoadFunction loader, FreeFunction unloader) {
      m_load = cr::move (loader);
      m_free = cr::move (unloader);
   }

public:
   uint8 *load (const String &file, int *size) {
      if (m_load) {
         return m_load (file.chars (), size);
      }
      return nullptr;
   }

   void unload (void *buffer) {
      if (m_free) {
         m_free (buffer);
      }
   }
};

class MemFile final : public DenyCopying {
private:
   static constexpr char kEOF = static_cast <char> (-1);

private:
   uint8 *m_data = nullptr;
   size_t m_length = 0, m_pos = 0;

public:
   explicit MemFile () = default;

   MemFile (const String &file) {
      open (file);
   }

   ~MemFile () {
      close ();
   }

public:
   bool open (const String &file) {
      m_length = 0;
      m_pos = 0;
      m_data = MemFileStorage::get ().load (file.chars (), reinterpret_cast <int *> (&m_length));

      if (!m_data) {
         return false;
      }
      return true;
   }

   void close () {
      MemFileStorage::get ().unload (m_data);

      m_length = 0;
      m_pos = 0;
      m_data = nullptr;
   }

   char getChar () {
      if (!m_data || m_pos >= m_length) {
         return kEOF;
      }
      auto ch = m_data[m_pos];
      ++m_pos;

      return static_cast <char> (ch);
   }

   char *getString (char *buffer, size_t count) {
      if (!m_data || m_pos >= m_length) {
         return nullptr;
      }
      size_t index = 0;
      buffer[0] = 0;

      for (; index < count - 1;) {
         if (m_pos < m_length) {
            buffer[index] = m_data[m_pos++];

            if (buffer[index++] == '\n') {
               break;
            }
         }
         else {
            break;
         }
      }
      buffer[index] = 0;
      return index ? buffer : nullptr;
   }

   bool getLine (String &line) {
      line.clear ();
      char ch;
      
      while ((ch = getChar ()) != kEOF) {
         line += ch;

         if (ch == '\n') {
            break;
         }
      }
      return !eof ();
   }

   size_t read (void *buffer, size_t size, size_t count = 1) {
      if (!m_data || m_length <= m_pos || !buffer || !size || !count) {
         return 0;
      }
      size_t blocks_read = size * count <= m_length - m_pos ? size * count : m_length - m_pos;

      memcpy (buffer, &m_data[m_pos], blocks_read);
      m_pos += blocks_read;

      return blocks_read / size;
   }

   bool seek (size_t offset, int origin) {
      if (!m_data || m_pos >= m_length) {
         return false;
      }
      if (origin == SEEK_SET) {
         if (offset >= m_length) {
            return false;
         }
         m_pos = offset;
      }
      else if (origin == SEEK_END) {
         if (offset >= m_length) {
            return false;
         }
         m_pos = m_length - offset;
      }
      else {
         if (m_pos + offset >= m_length) {
            return false;
         }
         m_pos += offset;
      }
      return true;
   }

   size_t length () const {
      return m_length;
   }

   bool eof () const {
      return m_pos >= m_length;
   }

   void rewind () {
      m_pos = 0;
   }

public:
   explicit operator bool () const {
      return !!m_data && m_length > 0;
   }
};

CR_NAMESPACE_END