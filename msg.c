#include "headers.h"

int verbosity;

void die (const char * fmt, ...) {
  fflush (stdout);
  fputs ("*** dualcount: ", stderr);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

void msg (int level, const char * fmt, ...) {
  if (verbosity < level) return;
  fputs ("c [dualcount] ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}
