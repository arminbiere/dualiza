#include "headers.h"

static Input * new_input (const char * name, FILE * file, int close) {
  Input * res;
  NEW (res);
  STRDUP (res->name, name);
  res->file = file;
  res->close = close;
  return res;
}

void delete_input (Input * input) {
  if (input->close == 1) fclose (input->file);
  if (input->close == 2) pclose (input->file);
  STRDEL (input->name);
  DELETE (input);
}

Input * new_input_from_stdin () {
  return new_input ("<stdin>", stdin, 0);
}

static int has_suffix (const char * s, const char * t) {
  size_t k = strlen (s), l = strlen (t);
  return k >= l && !strcmp (s + k - l, t);
}

static FILE * open_pipe (const char * name,
                         const char * suffix, const char * fmt)
{
  if (!has_suffix (name, suffix)) return 0;
  long bytes = strlen (fmt) + strlen (name);
  char * cmd;
  ALLOC (cmd, bytes);
  sprintf (cmd, fmt, name);
  FILE * res = popen (cmd, "r");
  DEALLOC (cmd, bytes);
  if (!res) die ("failed to open pipe to read compressed '%s'", name);
  return res;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int file_exists (const char * name) {
  struct stat buf;
  return !stat (name, &buf);
}

Input * open_new_input (const char * name) {
  if (!file_exists (name))
    die ("input file '%s' does not exist", name);
  int close = 2;
  FILE * file;
  if ((file = open_pipe (name, ".bz2", "bzip2 -c -d %s"))
  ||  (file = open_pipe (name, ".gz",  "gzip -c -d %s"))
  ||  (file = open_pipe (name, ".xz",  "xz -c -d %s"))
  ||  (file = open_pipe (name, ".7z",  "7z x -so %s 2>/dev/null")))
    msg (2, "opened pipe to read compressed file '%s'", name);
  else file = fopen (name, "r"), close = 1;
  if (!file) die ("failed to open '%s'", name);
  return new_input (name, file, close);
}
