//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
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
   File m_handle;
   PrintFunction m_printer;

public:
   SimpleLogger () = default;

   ~SimpleLogger () {
      m_handle.close ();
   }

private:
   void logToFile (const char *level, const char *msg) {
      if (!m_handle) {
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

      m_handle.puts ("%s (%s): %s\n", timebuf, level, msg);
   }

public:
   template <typename ...Args> void fatal (const char *fmt, Args ...args) {
      auto msg = strings.format (fmt, cr::forward <Args> (args)...);

      logToFile ("FATAL", msg);

      if (m_printer) {
         m_printer (msg);
      }
      plat.abort (msg);
   }

   template <typename ...Args> void error (const char *fmt, Args ...args) {
      auto msg = strings.format (fmt, cr::forward <Args> (args)...);

      logToFile ("ERROR", msg);

      if (m_printer) {
         m_printer (msg);
      }
   }

   template <typename ...Args> void message (const char *fmt, Args ...args) {
      auto msg = strings.format (fmt, cr::forward <Args> (args)...);

      logToFile ("INFO", msg);

      if (m_printer) {
         m_printer (msg);
      }
   }

public:
   void initialize (const String &filename, PrintFunction printFunction) {
      if (m_handle) {
         m_handle.close ();
      }
      m_printer = cr::move (printFunction);
      m_handle.open (filename, "at");
   }
};

// expose global instance
CR_EXPOSE_GLOBAL_SINGLETON (SimpleLogger, logger);

CR_NAMESPACE_END