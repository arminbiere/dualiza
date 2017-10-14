#ifndef NLOG

#include "headers.h"

void log_message (const char * fmt, ...) {
  assert (logging);
  fputs ("c LOG ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

#endif
