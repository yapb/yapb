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

#include <time.h>

#include <crlib/cr-files.h>
#include <crlib/cr-lambda.h>

CR_NAMESPACE_BEGIN

class SimpleLogger final : public Singleton <SimpleLogger> {
public:
   using PrintFunction = Lambda <void (const char *)>;

private:
   File handle_;
   PrintFunction printFun_;

public:
   SimpleLogger () = default;

   ~SimpleLogger () {
      handle_.close ();
   }

private:
   void logToFile (const char *level, const char *msg) {
      if (!handle_) {
         return;
      }
      time_t ticks = time (&ticks);
      tm *timeinfo = nullptr;

#if defined (CR_WINDOWS)
      tm get;

      localtime_s (&get, &ticks);
      timeinfo = &get;
#else
      timeinfo = localtime (&ticks);
#endif

      auto timebuf = strings.chars ();
      strftime (timebuf, StringBuffer::StaticBufferSize, "%Y-%m-%d %H:%M:%S", timeinfo);

      handle_.puts ("%s (%s): %s\n", timebuf, level, msg);
   }

public:
   template <typename ...Args> void fatal (const char *fmt, Args &&...args) {
      auto msg = strings.format (fmt, cr::forward <Args> (args)...);

      logToFile ("FATAL", msg);

      if (printFun_) {
         printFun_ (msg);
      }
      plat.abort (msg);
   }

   template <typename ...Args> void error (const char *fmt, Args &&...args) {
      auto msg = strings.format (fmt, cr::forward <Args> (args)...);

      logToFile ("ERROR", msg);

      if (printFun_) {
         printFun_ (msg);
      }
   }

   template <typename ...Args> void message (const char *fmt, Args &&...args) {
      auto msg = strings.format (fmt, cr::forward <Args> (args)...);

      logToFile ("INFO", msg);

      if (printFun_) {
         printFun_ (msg);
      }
   }

public:
   void initialize (StringRef filename, PrintFunction printFunction) {
      if (handle_) {
         handle_.close ();
      }
      printFun_ = cr::move (printFunction);
      handle_.open (filename, "at");
   }
};

// expose global instance
CR_EXPOSE_GLOBAL_SINGLETON (SimpleLogger, logger);

CR_NAMESPACE_END