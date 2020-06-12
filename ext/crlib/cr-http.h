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

#include <stdio.h>

#include <crlib/cr-string.h>
#include <crlib/cr-files.h>
#include <crlib/cr-logger.h>
#include <crlib/cr-twin.h>
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
#  include <ws2tcpip.h>
#endif

// TODO: make this work with real HTTP responses

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
)

CR_NAMESPACE_BEGIN

class Socket final : public DenyCopying {
private:
   int32 socket_;
   uint32 timeout_;

public:
   Socket () : socket_ (-1), timeout_ (2) {
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
   bool connect (StringRef hostname) {
      addrinfo  hints, *result = nullptr;
      plat.bzero (&hints, sizeof (hints));

      constexpr auto NumericServ = 0x00000008;

      hints.ai_flags = NumericServ;
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;

      if (getaddrinfo (hostname.chars (), "80", &hints, &result) != 0) {
         return false;
      }
      socket_ = static_cast <int> (socket (result->ai_family, result->ai_socktype, 0));

      if (socket_ < 0) {
         freeaddrinfo (result);
         return false;
      }

      auto getTimeouts = [&] () -> Twin <char *, int32> {
#if defined (CR_WINDOWS)
         DWORD tv = timeout_ * 1000;
#else
         timeval tv { static_cast <time_t> (timeout_), 0 };
#endif
         return { reinterpret_cast <char *> (&tv), static_cast <int32> (sizeof (tv)) };
      };
      auto timeouts = getTimeouts ();

      if (setsockopt (socket_, SOL_SOCKET, SO_RCVTIMEO, timeouts.first, timeouts.second) == -1) {
         logger.message ("Unable to set SO_RCVTIMEO.");
      }

      if (setsockopt (socket_, SOL_SOCKET, SO_SNDTIMEO, timeouts.first, timeouts.second) == -1) {
         logger.message ("Unable to set SO_SNDTIMEO.");
      }

      if (::connect (socket_, result->ai_addr, static_cast <int32> (result->ai_addrlen)) == -1) {
         disconnect ();
         freeaddrinfo (result);
         
         return false;
      }
      freeaddrinfo (result);

      return true;
   }

   void setTimeout (uint32 timeout) {
      timeout_ = timeout;
   }

   void disconnect () {
#if defined(CR_WINDOWS)
      if (socket_ != -1) {
         closesocket (socket_);
      }
#else 
      if (socket_ != -1)
         close (socket_);
#endif
   }

public:
   template <typename U> int32 send (const U *buffer, int32 length) const {
      return ::send (socket_, reinterpret_cast <const char *> (buffer), length, 0);
   }

   template <typename U> int32 recv (U *buffer, int32 length) {
      return ::recv (socket_, reinterpret_cast <char *> (buffer), length, 0);
   }

public:
   static int32 CR_STDCALL sendto (int socket, const void *message, size_t length, int flags, const struct sockaddr *dest, int32 destLength) {
#if defined (CR_WINDOWS)
      WSABUF buffer = { static_cast <ULONG> (length), const_cast <char *> (reinterpret_cast <const char *> (message)) };
      DWORD sendLength = 0;

      if (WSASendTo (socket, &buffer, 1, &sendLength, flags, dest, destLength, NULL, NULL) == SOCKET_ERROR) {
         errno = WSAGetLastError ();
         return -1;
      }
      return static_cast <int32> (sendLength);
#else
      iovec iov = { const_cast <void *> (message), length };
      msghdr msg {};

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
      static HttpUri parse (StringRef uri) {
         HttpUri result;

         if (uri.empty ()) {
            return result;
         }
         size_t protocol = uri.find ("://");

         if (protocol != String::InvalidIndex) {
            result.protocol = uri.substr (0, protocol);

            size_t hostIndex = uri.find ("/", protocol + 3);

            if (hostIndex != String::InvalidIndex) {
               result.path = uri.substr (hostIndex + 1);
               result.host = uri.substr (protocol + 3, hostIndex - protocol - 3);

               return result;
            }
         }
         return result;
      }
   };
}

// simple http client for downloading/uploading files only
class HttpClient final : public Singleton <HttpClient> {
private:
   enum : int32 {
      MaxReceiveErrors = 12
   };

private:
   Socket socket_;
   String userAgent_ = "crlib";
   HttpClientResult statusCode_ = HttpClientResult::Undefined;
   int32 chunkSize_ = 4096;

public:
   HttpClient () = default;
   ~HttpClient () = default;

private:
   HttpClientResult parseResponseHeader (uint8 *buffer) {
      bool isFinished = false;
      int32 pos = 0, symbols = 0, errors = 0;

      // prase response header
      while (!isFinished && pos < chunkSize_) {
         if (socket_.recv (&buffer[pos], 1) < 1) {
            if (++errors > MaxReceiveErrors) {
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
            ++symbols;
            break;
         }
         ++pos;
      }
      String response { reinterpret_cast <const char *> (buffer) };
      size_t responseCodeStart = response.find ("HTTP/1.1");

      if (responseCodeStart != String::InvalidIndex) {
         String respCode = response.substr (responseCodeStart + 9, 3);
         respCode.trim ();

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
   bool downloadFile (StringRef url, StringRef localPath) {
      if (File::exists (localPath.chars ())) {
         statusCode_ = HttpClientResult::LocalFileExists;
         return false;
      }
      auto uri = detail::HttpUri::parse (url);

      // no https...
      if (uri.protocol == "https") {
         statusCode_ = HttpClientResult::HttpOnly;
         return false;
      }

      // unable to connect...
      if (!socket_.connect (uri.host)) {
         statusCode_ = HttpClientResult::ConnectError;
         socket_.disconnect ();

         return false;
      }

      String request;
      request.appendf ("GET /%s HTTP/1.1\r\n", uri.path);
      request.append ("Accept: */*\r\n");
      request.append ("Connection: close\r\n");
      request.append ("Keep-Alive: 115\r\n");
      request.appendf ("User-Agent: %s\r\n", userAgent_);
      request.appendf ("Host: %s\r\n\r\n", uri.host);

      if (socket_.send (request.chars (), static_cast <int32> (request.length ())) < 1) {
         statusCode_ = HttpClientResult::SocketError;
         socket_.disconnect ();

         return false;
      }
      SmallArray <uint8> buffer (chunkSize_);
      statusCode_ = parseResponseHeader (buffer.data ());

      if (statusCode_ != HttpClientResult::OK) {
         socket_.disconnect ();
         return false;
      }

      // receive the file
      File file (localPath, "wb");

      if (!file) {
         statusCode_ = HttpClientResult::Undefined;
         socket_.disconnect ();

         return false;
      }
      int32 length = 0;
      int32 errors = 0;

      for (;;) {
         length = socket_.recv (buffer.data (), chunkSize_);

         if (length > 0) {
            file.write (buffer.data (), length);
         }
         else if (++errors > 12) {
            break;
         }
      }
      file.close ();

      socket_.disconnect ();
      statusCode_ = HttpClientResult::OK;

      return true;
   }

   bool uploadFile (StringRef url, StringRef localPath) {
      if (!File::exists (localPath.chars ())) {
         statusCode_ = HttpClientResult::NoLocalFile;
         return false;
      }
      auto uri = detail::HttpUri::parse (url);

      // no https...
      if (uri.protocol == "https") {
         statusCode_ = HttpClientResult::HttpOnly;
         return false;
      }

      // unable to connect...
      if (!socket_.connect (uri.host)) {
         statusCode_ = HttpClientResult::ConnectError;
         socket_.disconnect ();

         return false;
      }

      // receive the file
      File file (localPath, "rb");

      if (!file) {
         statusCode_ = HttpClientResult::Undefined;
         socket_.disconnect ();

         return false;
      }
      String boundaryName = localPath;
      size_t boundarySlash = localPath.findLastOf ("\\/");

      if (boundarySlash != String::InvalidIndex) {
         boundaryName = localPath.substr (boundarySlash + 1);
      }
      StringRef boundaryLine = "---crlib_upload_boundary_1337";

      String request, start, end;
      start.appendf ("--%s\r\n", boundaryLine);
      start.appendf ("Content-Disposition: form-data; name='file'; filename='%s'\r\n", boundaryName);
      start.append ("Content-Type: application/octet-stream\r\n\r\n");

      end.appendf ("\r\n--%s--\r\n\r\n", boundaryLine);

      request.appendf ("POST /%s HTTP/1.1\r\n", uri.path);
      request.appendf ("Host: %s\r\n", uri.host);
      request.appendf ("User-Agent: %s\r\n", userAgent_);
      request.appendf ("Content-Type: multipart/form-data; boundary=%s\r\n", boundaryLine);
      request.appendf ("Content-Length: %d\r\n\r\n", file.length () + start.length () + end.length ());

      // send the main request
      if (socket_.send (request.chars (), static_cast <int32> (request.length ())) < 1) {
         statusCode_ = HttpClientResult::SocketError;
         socket_.disconnect ();

         return false;
      }

      // send boundary start
      if (socket_.send (start.chars (), static_cast <int32> (start.length ())) < 1) {
         statusCode_ = HttpClientResult::SocketError;
         socket_.disconnect ();

         return false;
      }
      SmallArray <uint8> buffer (chunkSize_);
      int32 length = 0;

      for (;;) {
         length = static_cast <int32> (file.read (buffer.data (), 1, chunkSize_));

         if (length > 0) {
            socket_.send (buffer.data (), length);
         }
         else {
            break;
         }
      }

      // send boundary end
      if (socket_.send (end.chars (), static_cast <int32> (end.length ())) < 1) {
         statusCode_ = HttpClientResult::SocketError;
         socket_.disconnect ();

         return false;
      }
      statusCode_ = parseResponseHeader (buffer.data ());
      socket_.disconnect ();

      return statusCode_ == HttpClientResult::OK;
   }

public:
   void setUserAgent (StringRef ua) {
      userAgent_ = ua;
   }

   HttpClientResult getLastStatusCode () {
      return statusCode_;
   }

   void setChunkSize (int32 chunkSize) {
      chunkSize_ = chunkSize;
   }

   void setTimeout (uint32 timeout) {
      socket_.setTimeout (timeout);
   }
};

// expose global http client
CR_EXPOSE_GLOBAL_SINGLETON (HttpClient, http);

CR_NAMESPACE_END
