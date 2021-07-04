#include "headers.h"

static Writer *
new_writer (const char *name, FILE * file, int close)
{
  assert (name);
  assert (file);
  Writer *res;
  NEW (res);
  STRDUP (res->name, name);
  res->close = close;
  res->file = file;
  return res;
}

Writer *
new_writer_from_stdout ()
{
  return new_writer ("<stdout>", stdout, 0);
}

static FILE *
match_and_write_pipe (const char *name, const char *suffix, const char *fmt)
{
  if (!has_suffix (name, suffix))
    return 0;
  return write_pipe (fmt, name);
}

Writer *
open_new_writer (const char *name)
{
  int close = 2;
  FILE *file;
  if ((file = match_and_write_pipe (name, ".bz2", "bzip2 -c > %s"))
      || (file = match_and_write_pipe (name, ".gz", "gzip -c > %s"))
      || (file = match_and_write_pipe (name, ".xz", "xz -c > %s"))
      || (file = match_and_write_pipe (name, ".7z",
				       "7z a -an -txz -si -so > %s 2>/dev/null")))
    msg (2, "opened pipe to write compressed file '%s'", name);
  else
    file = fopen (name, "w"), close = 1;
  if (!file)
    die ("failed to open '%s'", name);
  return new_writer (name, file, close);
}

void
delete_writer (Writer * writer)
{
  if (writer->close == 1)
    fclose (writer->file);
  if (writer->close == 2)
    pclose (writer->file);
  STRDEL (writer->name);
  DELETE (writer);
}
