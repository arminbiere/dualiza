#include "headers.h"

const char *message_prefix = "c ";
FILE *message_file;

void
die (const char *fmt, ...)
{
  fflush (stdout);
  fputs ("dualiza: error: ", stderr);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

void
msg (int level, const char *fmt, ...)
{
  if (options.verbosity < level)
    return;
  if (!message_file)
    message_file = stdout;
  if (message_prefix)
    fputs (message_prefix, message_file);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (message_file, fmt, ap);
  va_end (ap);
  fputc ('\n', message_file);
  fflush (message_file);
}
