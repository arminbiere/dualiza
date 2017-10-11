#include "headers.h"

Reader * new_reader (const char * name, FILE * file) {
  assert (name);
  assert (file);
  Reader * res;
  NEW (res);
  res->name = name;
  res->file = file;
  res->lineno = 1;
  return res;
}

void delete_reader (Reader * reader) {
  DELETE (reader);
}

int next_char (Reader * reader) {
  int res;
  if (reader->buffered) res = reader->buffer, reader->buffered = 0;
  else res = getc (reader->file);
  if (res == '\n') reader->lineno++;
  if (res != EOF) reader->bytes++;
  return res;
}

int next_non_white_space_char (Reader * reader) {
  int ch;
  while (isspace ((ch = next_char (reader))))
    ;
  return ch;
}

void prev_char (Reader * reader, int ch) {
  assert (!reader->buffered);
  reader->buffered = 1;
  reader->buffer = ch;
  if (ch == '\n') assert (reader->lineno > 1), reader->lineno--;
  if (ch != EOF) assert (reader->bytes > 0), reader->bytes--;
}

void parse_error (Reader * reader, const char * fmt, ...) {
  fflush (stdout);
  fprintf (stderr,
    "*** dualcount: parse error in '%s' line %d byte %d: ",
    reader->name, reader->lineno, reader->bytes);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}
