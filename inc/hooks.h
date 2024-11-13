//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// simple handler for parsing and rewriting queries (fake queries)
class QueryBuffer {
   SmallArray <uint8_t> m_buffer {};
   size_t m_cursor {};

public:
   QueryBuffer (const uint8_t *msg, size_t length, size_t shift) : m_cursor (0) {
      m_buffer.insert (0, msg, length);
      m_cursor += shift;
   }

public:
   template <typename T> T read () {
      T result {};
      constexpr auto size = sizeof (T);

      if (m_cursor + size > m_buffer.length ()) {
         return 0;
      }

      memcpy (&result, m_buffer.data () + m_cursor, size);
      m_cursor += size;

      return result;
   }

   // must be called right after read
   template <typename T> void write (T value) {
      constexpr auto size = sizeof (value);
      memcpy (m_buffer.data () + m_cursor - size, &value, size);
   }

   template <typename T> void skip () {
      constexpr auto size = sizeof (T);

      if (m_cursor + size > m_buffer.length ()) {
         return;
      }
      m_cursor += size;
   }

   void skipString () {
      if (m_buffer.length () < m_cursor) {
         return;
      }
      for (; m_cursor < m_buffer.length () && m_buffer[m_cursor] != kNullChar; ++m_cursor) {}
      ++m_cursor;
   }


   String readString () {
      if (m_buffer.length () < m_cursor) {
         return "";
      }
      String out;

      for (; m_cursor < m_buffer.length () && m_buffer[m_cursor] != kNullChar; ++m_cursor) {
         out += m_buffer[m_cursor];
      }
      ++m_cursor;

      return out;
   }

   void shiftToEnd () {
      m_cursor = m_buffer.length ();
   }

public:
   Twin <const uint8_t *, size_t> data () {
      return { m_buffer.data (), m_buffer.length () };
   }
};

// used for response with fake timestamps and bots count in server responses
class ServerQueryHook : public Singleton <ServerQueryHook> {
private:
   using SendToProto = decltype (sendto);

private:
   Detour <SendToProto> m_sendToDetour {}, m_sendToDetourSys {};

public:
   ServerQueryHook () = default;
   ~ServerQueryHook () = default;

public:
   // initialzie and install hook
   void init ();

public:
   // disables send hook
   bool disable () {
      m_sendToDetourSys.restore ();
      return m_sendToDetour.restore ();
   }

public:
   static int32_t CR_STDCALL sendTo (int socket, const void *message, size_t length, int flags, const struct sockaddr *dest, int destLength);
};

// used for transit calls between game dll and engine without all needed functions on bot side
class DynamicLinkerHook : public Singleton <DynamicLinkerHook> {
private:
#if defined (CR_WINDOWS)
#  define DLSYM_FUNCTION GetProcAddress
#  define DLCLOSE_FUNCTION FreeLibrary
#else
#  define DLSYM_FUNCTION dlsym
#  define DLCLOSE_FUNCTION dlclose
#endif

private:
   using DlsymProto = SharedLibrary::Func CR_STDCALL (SharedLibrary::Handle, const char *);
   using DlcloseProto = int CR_STDCALL (SharedLibrary::Handle);

private:
   bool m_paused { false };

   Detour <DlsymProto> m_dlsym {};
   Detour <DlcloseProto> m_dlclose {};
   HashMap <StringRef, SharedLibrary::Func> m_exports {};

   SharedLibrary m_self {};

public:
   DynamicLinkerHook () = default;
   ~DynamicLinkerHook () = default;

public:
   void initialize ();
   bool needsBypass () const;

   SharedLibrary::Func lookup (SharedLibrary::Handle module, const char *function);

   decltype (auto) close (SharedLibrary::Handle module) {
      if (m_self.handle () == module) {
         disable ();
         return m_dlclose (module);
      }
      return m_dlclose (module);
   }

public:
   bool callPlayerFunction (edict_t *ent);

public:
   void enable () {
      if (m_dlsym.detoured ()) {
         return;
      }
      m_dlsym.detour ();
   }

   void disable () {
      if (!m_dlsym.detoured ()) {
         return;
      }
      m_dlsym.restore ();
   }

   void setPaused (bool what) {
      m_paused = what;
   }

   bool isPaused () const {
      return m_paused;
   }

public:
   static SharedLibrary::Func CR_STDCALL lookupHandler (SharedLibrary::Handle module, const char *function) {
      return instance ().lookup (module, function);
   }

   static int CR_STDCALL closeHandler (SharedLibrary::Handle module) {
      return instance ().close (module);
   }
};

// expose global
CR_EXPOSE_GLOBAL_SINGLETON (DynamicLinkerHook, entlink);
CR_EXPOSE_GLOBAL_SINGLETON (ServerQueryHook, fakequeries);
