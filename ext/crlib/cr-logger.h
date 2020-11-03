//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
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
      tm timeinfo {};

#if defined (CR_WINDOWS)
      localtime_s (&timeinfo, &ticks);
#else
      localtime_r (&ticks, &timeinfo);
#endif

      auto timebuf = strings.chars ();
      strftime (timebuf, StringBuffer::StaticBufferSize, "%Y-%m-%d %H:%M:%S", &timeinfo);

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
