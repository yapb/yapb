//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
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
      auto tm = localtime (&ticks);

      auto timebuf = strings.chars ();
      strftime (timebuf, StringBuffer::StaticBufferSize, "%Y-%m-%d %H:%M:%S", tm);

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
static auto &logger = SimpleLogger::get ();

CR_NAMESPACE_END