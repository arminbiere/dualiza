#include "headers.h"

static Reader * new_reader (const char * name, FILE * file, int close) {
  assert (name);
  assert (file);
  Reader * res;
  NEW (res);
  res->name = name;
  res->buffer = new_buffer ();
  res->close = close;
  res->file = file;
  res->lineno = 1;
  return res;
}

Reader * new_reader_from_stdin () {
  return new_reader ("<stdin>", stdin, 0);
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

Reader * open_new_reader (const char * name) {
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
  return new_reader (name, file, close);
}

void delete_reader (Reader * r) {
  delete_buffer (r->buffer);
  if (r->close == 1) fclose (r->file);
  if (r->close == 2) pclose (r->file);
  RELEASE (r->symbol);
  DELETE (r);
}

int next_char (Reader * r) {
  int res;
  if (empty_buffer (r->buffer)) res = getc (r->file);
  else res = dequeue_buffer (r->buffer);
  if (res == '\n') r->lineno++;
  if (res != EOF) r->bytes++;
  return res;
}

static int is_space_character (int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\n';
}

int next_non_white_space_char (Reader * r) {
  int ch;
  for (;;) {
    while (is_space_character (ch = next_char (r)))
      ;
    if (!r->comment || ch != r->comment) break;
    while ((ch = next_char (r)) != '\n')
      if (ch == EOF)
	parse_error (r, "unexpected end-of-file in comment");
  }
  return ch;
}

void prev_char (Reader * r, int ch) {
  enqueue_buffer (r->buffer, ch);
  if (ch == '\n') assert (r->lineno > 1), r->lineno--;
  if (ch != EOF) assert (r->bytes > 0), r->bytes--;
}

void parse_error (Reader * r, const char * fmt, ...) {
  fflush (stdout);
  fprintf (stderr,
    "dualcount: %s:%d: parse error at byte %d: ",
    r->name, r->lineno, r->bytes);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}
