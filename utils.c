#include "headers.h"

int has_suffix (const char * s, const char * t) {
  size_t k = strlen (s), l = strlen (t);
  return k >= l && !strcmp (s + k - l, t);
}

static FILE * open_pipe (int write, const char * fmt, const char * arg) {
  long bytes = strlen (fmt);
  if (arg) bytes += strlen (arg);
  char * cmd;
  ALLOC (cmd, bytes);
  if (arg) sprintf (cmd, fmt, arg);
  else strcpy (cmd, fmt);
  const char * type = write ? "w" : "r";
  FILE * res = popen (cmd, type);
  DEALLOC (cmd, bytes);
  if (!res) die ("'popen (\"%s\", \"%s\")' failed", cmd, type);
  return res;
}

FILE * read_pipe (const char * fmt, const char * arg) {
  return open_pipe (0, fmt, arg);
}

FILE * write_pipe (const char * fmt, const char * arg) {
  return open_pipe (1, fmt, arg);
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int file_exists (const char * name) {
  struct stat buf;
  return !stat (name, &buf);
}
