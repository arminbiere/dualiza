#include "headers.h"

static Reader * new_reader (const char * name, FILE * file, int close) {
  assert (name);
  assert (file);
  Reader * res;
  NEW (res);
  STRDUP (res->name, name);
  res->buffer = new_buffer ();
  res->close = close;
  res->file = file;
  res->lineno = 1;
  return res;
}

Reader * new_reader_from_stdin () {
  return new_reader ("<stdin>", stdin, 0);
}

static FILE * match_and_read_pipe (const char * name,
				   const char * suffix,
				   const char * fmt) {
  if (!has_suffix (name, suffix)) return 0;
  return read_pipe (fmt, name);
}

Reader * open_new_reader (const char * name) {
  if (!file_exists (name))
    die ("input file '%s' does not exist", name);
  int close = 2;
  FILE * file;
  if ((file = match_and_read_pipe (name, ".bz2", "bzip2 -c -d %s"))
  ||  (file = match_and_read_pipe (name, ".gz",  "gzip -c -d %s"))
  ||  (file = match_and_read_pipe (name, ".xz",  "xz -c -d %s"))
  ||  (file = match_and_read_pipe (name, ".7z",  "7z x -so %s 2>/dev/null")))
    msg (2, "opened pipe to read compressed file '%s'", name);
  else file = fopen (name, "r"), close = 1;
  if (!file) die ("failed to open '%s'", name);
  return new_reader (name, file, close);
}

void delete_reader (Reader * r) {
  msg (2,
    "deleting input reader of '%s' after reading %ld bytes",
    r->name, r->bytes);
  delete_buffer (r->buffer);
  if (r->close == 1) fclose (r->file);
  if (r->close == 2) pclose (r->file);
  STRDEL (r->name);
  RELEASE (r->symbol);
  DELETE (r);
}

static int get_char (Reader * r) {
  if (r->eof) return EOF;
  int res = getc (r->file);
  if (res == EOF) r->eof = 1;
  return res;
}

int next_char (Reader * r) {
  int res;
  if (empty_buffer (r->buffer)) res = get_char (r);
  else res = dequeue_buffer (r->buffer);
  if (res == '\n') r->lineno++;
  if (res != EOF) r->bytes++;
  return res;
}

int peek_char (Reader * r) {
  int res = get_char (r);
  if (res != EOF) enqueue_buffer (r->buffer, res);
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
    "dualiza: %s:%ld: parse error at byte %ld: ",
    r->name, r->lineno, r->bytes);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}
