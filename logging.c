#ifndef NLOG

#include "headers.h"

void log_message (const char * fmt, ...) {
  assert (options.logging);
  fputs ("c LOG ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

void log_clause (Clause * c, const char * fmt, ...) {
  assert (options.logging);
  fputs ("c LOG ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputs (c->dual ? " dual" : " primal", stdout);
  if (c->redundant) printf (" redundant glue %d", c->glue);
  printf (" size %d clause[%ld]", c->size, (long) c->id);
  for (int i = 0; i < c->size; i++)
    printf (" %d", c->literals[i]);
  fputc ('\n', stdout);
  fflush (stdout);
}

#endif
