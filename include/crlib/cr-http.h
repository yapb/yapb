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
#include <crlib/cr-files.h>
#include <crlib/cr-logger.h>
#include <crlib/cr-platform.h>

#if defined (CR_LINUX) || defined (CR_OSX)
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <sys/uio.h> 
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <errno.h>
#  include <netdb.h>
#  include <fcntl.h>
#elif defined (CR_WINDOWS)
#  include <winsock2.h>
#endif

// status codes for http client
CR_DECLARE_SCOPED_ENUM (HttpClientResult,
   OK = 0,
   NotFound,
   Forbidden,
   SocketError,
   ConnectError,
   HttpOnly,
   Undefined,
   NoLocalFile = -1,
   LocalFileExists = -2
);

CR_NAMESPACE_BEGIN

class Socket final : public DenyCopying {
private:
   int32 m_socket;
   uint32 m_timeout;

public:
   Socket () : m_socket (-1), m_timeout (2) {
#if defined(CR_WINDOWS)
      WSADATA wsa;

      if (WSAStartup (MAKEWORD (2, 2), &wsa) != 0) {
         logger.error ("Unable to inialize sockets.");
      }
#endif
   }

   ~Socket () {
      disconnect ();
#if defined (CR_WINDOWS)
      WSACleanup ();
#endif
   }


public:
   bool connect (const String &hostname) {
      auto host = gethostbyname (hostname.chars ());

      if (!host) {
         return false;
      }
      m_socket = static_cast <int> (socket (AF_INET, SOCK_STREAM, 0));

      if (m_socket < 0) {
         return false;
      }

      auto getTimeouts = [&] () -> Twin <char *, size_t> {
#if defined (CR_WINDOWS)
         DWORD tv = m_timeout * 1000;
#else
         timeval tv { static_cast <time_t> (m_timeout), 0 };
#endif
         return { reinterpret_cast <char *> (&tv), sizeof (tv) };
      };
      auto timeouts = getTimeouts ();

      if (setsockopt (m_socket, SOL_SOCKET, SO_RCVTIMEO, timeouts.first, timeouts.second) == -1) {
         logger.error ("Unable to set SO_RCVTIMEO.");
      }

      if (setsockopt (m_socket, SOL_SOCKET, SO_SNDTIMEO, timeouts.first, timeouts.second) == -1) {
         logger.error ("Unable to set SO_SNDTIMEO.");
      }

      sockaddr_in dest;
      memset (&dest, 0, sizeof (dest));

      dest.sin_family = AF_INET;
      dest.sin_port = htons (80);
      dest.sin_addr.s_addr = inet_addr (inet_ntoa (*(reinterpret_cast <in_addr *> (host->h_addr))));

      if (::connect (m_socket, reinterpret_cast <sockaddr *> (&dest), static_cast <int> (sizeof (dest))) == -1) {
         disconnect ();
         return false;
      }
      return true;
   }

   void setTimeout (uint32 timeout) {
      m_timeout = timeout;
   }

   void disconnect () {
#if defined(CR_WINDOWS)
      if (m_socket != -1) {
         closesocket (m_socket);
      }
#else 
      if (m_socket != -1)
         close (m_socket);
#endif
   }

public:
   template <typename U> int32 send (const U *buffer, int32 length) const {
      return ::send (m_socket, reinterpret_cast <const char *> (buffer), length, 0);
   }

   template <typename U> int32 recv (U *buffer, int32 length) {
      return ::recv (m_socket, reinterpret_cast <char *> (buffer), length, 0);
   }

public:
   static int32 CR_STDCALL sendto (int socket, const void *message, size_t length, int flags, const struct sockaddr *dest, int32 destLength) {
#if defined (CR_WINDOWS)
      WSABUF buffer = { length, const_cast <char *> (reinterpret_cast <const char *> (message)) };
      DWORD sendLength = 0;

      if (WSASendTo (socket, &buffer, 1, &sendLength, flags, dest, destLength, NULL, NULL) == SOCKET_ERROR) {
         errno = WSAGetLastError ();
         return -1;
      }
      return static_cast <int32> (sendLength);
#else
      iovec iov = { const_cast <void *> (message), length };
      msghdr msg = { 0, };

      msg.msg_name = reinterpret_cast <void *> (const_cast <struct sockaddr *> (dest));
      msg.msg_namelen = destLength;
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;

      return sendmsg (socket, &msg, flags);
#endif
   }
};

namespace detail {

   // simple http uri omitting query-string and port
   struct HttpUri {
      String path, protocol, host;

   public:
      static HttpUri parse (const String &uri) {
         HttpUri result;

         if (uri.empty ()) {
            return result;
         }
         size_t protocol = uri.find ("://");

         if (protocol != String::kInvalidIndex) {
            result.protocol = uri.substr (0, protocol);

            size_t host = uri.find ("/", protocol + 3);

            if (host != String::kInvalidIndex) {
               result.path = uri.substr (host + 1);
               result.host = uri.substr (protocol + 3, host - protocol - 3);

               return result;
            }
         }
         return result;
      }
   };
};

// simple http client for downloading/uploading files only
class HttpClient final : public Singleton <HttpClient> {
private:
   static constexpr int32 kMaxRecvErrors = 12;

private:
   Socket m_socket;
   String m_userAgent = "crlib";
   HttpClientResult m_code = HttpClientResult::Undefined;
   int32 m_chunkSize = 4096;

public:
   HttpClient () = default;
   ~HttpClient () = default;

private:
   HttpClientResult parseResponseHeader (uint8 *buffer) {
      bool isFinished = false;
      int32 pos = 0, symbols = 0, errors = 0;

      // prase response header
      while (!isFinished && pos < m_chunkSize) {
         if (m_socket.recv (&buffer[pos], 1) < 1) {
            if (++errors > kMaxRecvErrors) {
               isFinished = true;
            }
            else {
               continue;
            }
         }

         switch (buffer[pos]) {
         case '\r':
            break;

         case '\n':
            isFinished = (symbols == 0);
            symbols = 0;
            break;

         default:
            symbols++;
            break;
         }
         pos++;
      }
      String response (reinterpret_cast <const char *> (buffer));
      size_t responseCodeStart = response.find ("HTTP/1.1");

      if (responseCodeStart != String::kInvalidIndex) {
         String respCode = response.substr (responseCodeStart + 9, 3).trim ();

         if (respCode == "200") {
            return HttpClientResult::OK;
         }
         else if (respCode == "403") {
            return HttpClientResult::Forbidden;
         }
         else if (respCode == "404") {
            return HttpClientResult::NotFound;
         }
      }
      return HttpClientResult::NotFound;
   }

public:

   // simple blocked download
   bool downloadFile (const String &url, const String &localPath) {
      if (File::exists (localPath.chars ())) {
         m_code = HttpClientResult::LocalFileExists;
         return false;
      }
      auto uri = detail::HttpUri::parse (url);

      // no https...
      if (uri.protocol == "https") {
         m_code = HttpClientResult::HttpOnly;
         return false;
      }

      // unable to connect...
      if (!m_socket.connect (uri.host)) {
         m_code = HttpClientResult::ConnectError;
         m_socket.disconnect ();

         return false;
      }

      String request;
      request.appendf ("GET /%s HTTP/1.1\r\n", uri.path.chars ());
      request.append ("Accept: */*\r\n");
      request.append ("Connection: close\r\n");
      request.append ("Keep-Alive: 115\r\n");
      request.appendf ("User-Agent: %s\r\n", m_userAgent.chars ());
      request.appendf ("Host: %s\r\n\r\n", uri.host.chars ());

      if (m_socket.send (request.chars (), static_cast <int32> (request.length ())) < 1) {
         m_code = HttpClientResult::SocketError;
         m_socket.disconnect ();

         return false;
      }
      SmallArray <uint8> buffer (m_chunkSize);
      m_code = parseResponseHeader (buffer.data ());

      if (m_code != HttpClientResult::OK) {
         m_socket.disconnect ();
         return false;
      }

      // receive the file
      File file (localPath, "wb");

      if (!file) {
         m_code = HttpClientResult::Undefined;
         m_socket.disconnect ();

         return false;
      }
      int32 length = 0;
      int32 errors = 0;

      for (;;) {
         length = m_socket.recv (buffer.data (), m_chunkSize);

         if (length > 0) {
            file.write (buffer.data (), length);
         }
         else if (++errors > 12) {
            break;
         }
      }
      file.close ();

      m_socket.disconnect ();
      m_code = HttpClientResult::OK;

      return true;
   }

   bool uploadFile (const String &url, const String &localPath) {
      if (!File::exists (localPath.chars ())) {
         m_code = HttpClientResult::NoLocalFile;
         return false;
      }
      auto uri = detail::HttpUri::parse (url);

      // no https...
      if (uri.protocol == "https") {
         m_code = HttpClientResult::HttpOnly;
         return false;
      }

      // unable to connect...
      if (!m_socket.connect (uri.host)) {
         m_code = HttpClientResult::ConnectError;
         m_socket.disconnect ();

         return false;
      }

      // receive the file
      File file (localPath, "rb");

      if (!file) {
         m_code = HttpClientResult::Undefined;
         m_socket.disconnect ();

         return false;
      }
      String boundaryName = localPath;
      size_t boundarySlash = localPath.findLastOf ("\\/");

      if (boundarySlash != String::kInvalidIndex) {
         boundaryName = localPath.substr (boundarySlash + 1);
      }
      const String &kBoundary = "---crlib_upload_boundary_1337";

      String request, start, end;
      start.appendf ("--%s\r\n", kBoundary.chars ());
      start.appendf ("Content-Disposition: form-data; name='file'; filename='%s'\r\n", boundaryName.chars ());
      start.append ("Content-Type: application/octet-stream\r\n\r\n");

      end.appendf ("\r\n--%s--\r\n\r\n", kBoundary.chars ());

      request.appendf ("POST /%s HTTP/1.1\r\n", uri.path.chars ());
      request.appendf ("Host: %s\r\n", uri.host.chars ());
      request.appendf ("User-Agent: %s\r\n", m_userAgent.chars ());
      request.appendf ("Content-Type: multipart/form-data; boundary=%s\r\n", kBoundary.chars ());
      request.appendf ("Content-Length: %d\r\n\r\n", file.length () + start.length () + end.length ());

      // send the main request
      if (m_socket.send (request.chars (), static_cast <int32> (request.length ())) < 1) {
         m_code = HttpClientResult::SocketError;
         m_socket.disconnect ();

         return false;
      }

      // send boundary start
      if (m_socket.send (start.chars (), static_cast <int32> (start.length ())) < 1) {
         m_code = HttpClientResult::SocketError;
         m_socket.disconnect ();

         return false;
      }
      SmallArray <uint8> buffer (m_chunkSize);
      int32 length = 0;

      for (;;) {
         length = static_cast <int32> (file.read (buffer.data (), 1, m_chunkSize));

         if (length > 0) {
            m_socket.send (buffer.data (), length);
         }
         else {
            break;
         }
      }

      // send boundary end
      if (m_socket.send (end.chars (), static_cast <int32> (end.length ())) < 1) {
         m_code = HttpClientResult::SocketError;
         m_socket.disconnect ();

         return false;
      }
      m_code = parseResponseHeader (buffer.data ());
      m_socket.disconnect ();

      return m_code == HttpClientResult::OK;
   }

public:
   void setUserAgent (const String &ua) {
      m_userAgent = ua;
   }

   HttpClientResult getLastStatusCode () {
      return m_code;
   }

   void setChunkSize (int32 chunkSize) {
      m_chunkSize = chunkSize;
   }

   void setTimeout (uint32 timeout) {
      m_socket.setTimeout (timeout);
   }
};

// expose global http client
static auto &http = HttpClient::get ();

CR_NAMESPACE_END