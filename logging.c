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

void log_clause (Clause * c, const char * fmt, ...) {
  assert (logging);
  fputs ("c LOG ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  if (c->negative) fputs (" negative", stdout);
  if (c->redundant) fputs (" redundant", stdout);
  printf (" size %d clause", c->size);
  for (int i = 0; i < c->size; i++)
    printf (" %d", c->literals[i]);
  fputc ('\n', stdout);
  fflush (stdout);
}

#endif
