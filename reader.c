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

void delete_reader (Reader * r) {
  RELEASE (r->symbol);
  DELETE (r);
}

int next_char (Reader * r) {
  int res;
  if (r->buffered) res = r->buffer, r->buffered = 0;
  else res = getc (r->file);
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
  assert (!r->buffered);
  r->buffered = 1;
  r->buffer = ch;
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
