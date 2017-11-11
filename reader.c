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
    r->name, r->coo.bytes);
  delete_buffer (r->buffer);
  if (r->close == 1) fclose (r->file);
  if (r->close == 2) pclose (r->file);
  STRDEL (r->name);
  RELEASE (r->symbol);
  DELETE (r);
}

static Char get_char (Reader * r) {
  Char res;
  if (r->char_saved) {
    res = r->saved_char;
    r->char_saved = 0;
  } else {
    res.coo = r->coo;
    if (r->eof) res.code = EOF;
    else {
      res.code = getc (r->file);
      if (res.code == EOF) r->eof = 1;
    }
  }
  return res;
}

Char next_char (Reader * r) {
  Char res;
  if (empty_buffer (r->buffer)) res = get_char (r);
  else res = dequeue_buffer (r->buffer);
  if (res.code == '\n') r->coo.line++, r->coo.column = 0;
  if (res.code != EOF) r->coo.bytes++;
  return res;
}

int peek_char (Reader * r) {
  Char res = get_char (r);
  if (res.code != EOF) enqueue_buffer (r->buffer, res);
  return res.code;
}

static int is_space_character (int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\n';
}

Char next_non_white_space_char (Reader * r) {
  Char ch;
  for (;;) {
    while (is_space_character ((ch = next_char (r)).code))
      ;
    if (ch.code == 'c' && r->type == DIMACS) goto SKIP_REST_OF_LINE;
    else if (ch.code == '-' && r->type == FORMULA) {
      if (next_char (r).code != '-')
	parse_error (r, "expected '-' after '-'");
    } else break;
SKIP_REST_OF_LINE:
    while (((ch = next_char (r)).code) != '\n')
      if (ch.code == EOF)
	parse_error (r, "unexpected end-of-file in comment");
  }
  return ch;
}

void prev_char (Reader * r, Char ch) {
  assert (!r->char_saved);
  r->char_saved = 1;
  r->saved_char = ch;
  r->coo = ch.coo;
}

void parse_error (Reader * r, const char * fmt, ...) {
  fflush (stdout);
  fprintf (stderr,
    "dualiza: %s:%ld:%ld: parse error at byte %ld: ",
    r->name, r->coo.line + 1, r->coo.column + 1, r->coo.bytes + 1);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

Type get_file_type (Reader * reader) {
  assert (reader);
  msg (2, "trying to determine file type of input file '%s'", reader->name);
  Type res = FORMULA;
  int ch = peek_char (reader);
  long count = 1;
  if (ch == 'a') {
    ch = peek_char (reader), count++;
    if (ch == 'i' || ch == 'a') {
      ch = peek_char (reader), count++;
      if (ch == 'g') {
	ch = peek_char (reader), count++;
	if (ch == ' ') {
	  ch = peek_char (reader), count++;
	  if ('0' <= ch && ch <= '9') res = AIGER;
	}
      }
    }
  } else if (ch == 'c' || ch == 'p') {
    while (ch == 'c') {
      do ch = peek_char (reader), count++;
      while (ch != EOF && ch != '\n');
      if (ch == '\n') ch = peek_char (reader), count++;
    }
    if (ch == 'p') {
      ch = peek_char (reader), count++;
      if (ch == ' ') {
	ch = peek_char (reader), count++;
	if (ch == 'c') res = DIMACS;
      }
    }
  }
  msg (2, "assuming %s file type after peeking at %ld characters",
    (res==AIGER ? "AIGER" : (res==DIMACS ? "DIMACS" : "formula")), count);
  reader->type = res;
  return res;
}
