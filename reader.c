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
  res.coo = r->coo;
  if (r->eof) res.code = EOF;
  else {
    res.code = getc (r->file);
    if (res.code == EOF) r->eof = 1;
  }
  return res;
}

static void inc_coo (Coo * c, int ch) {
  if (ch == '\n') c->line++, c->column = 0;
  else if (ch != EOF) c->column++;
  if (ch != EOF) c->bytes++;
}

Char next_char (Reader * r) {
  Char res;
  if (r->char_saved)
    res = r->saved_char, r->char_saved = 0, r->coo = res.coo;
  else if (empty_buffer (r->buffer)) res = get_char (r);
  else res = dequeue_buffer (r->buffer), r->coo = res.coo;
  inc_coo (&r->coo, res.code);
  return res;
}

int peek_char (Reader * r) {
  Char res = get_char (r);
  inc_coo (&r->coo, res.code);
  if (res.code != EOF) enqueue_buffer (r->buffer, res);
  return res.code;
}

Char next_non_white_space_char (Reader * r) {
  Char ch;
  for (;;) {
    while (is_space_character ((ch = next_char (r)).code))
      ;
    if (ch.code == 'c' && r->info == DIMACS) goto SKIP_REST_OF_LINE;
    else if (ch.code == '-' && r->info == FORMULA) {
      if ((ch = next_char (r)).code == '-') goto SKIP_REST_OF_LINE;
      if (ch.code == '>') {
	ch.code = IMPLIES;
	break;
      }
      else parse_error (r, ch, "expected '-' or '>' after '-'");
    } else if (ch.code == '<' && r->info == FORMULA) {
      if ((ch = next_char (r)).code != '-')
	parse_error (r, ch, "expected '-' after '<'");
      if ((ch = next_char (r)).code != '>')
	parse_error (r, ch, "expected '>' after '<-'");
      ch.code = IFF;
      break;
    } else break;
SKIP_REST_OF_LINE:
    while (((ch = next_char (r)).code) != '\n')
      if (ch.code == EOF)
	parse_error (r, ch, "unexpected end-of-file in comment");
  }
  return ch;
}

void prev_char (Reader * r, Char ch) {
  assert (!r->char_saved);
  r->char_saved = 1;
  r->saved_char = ch;
  r->coo = ch.coo;
}

void parse_error (Reader * r, Char ch, const char * fmt, ...) {
  fflush (stdout);
  long line = ch.coo.line;
  long column = ch.coo.column;
  long bytes = ch.coo.bytes;
  if (ch.code == IMPLIES) column--, bytes--;
  if (ch.code == IFF) column -= 2, bytes -= 2;
  fprintf (stderr,
    "dualiza: %s:%ld:%ld: parse error at byte %ld: ",
    r->name, line + 1, column + 1, bytes + 1);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

Info get_file_info (Reader * reader) {
  assert (reader);
  msg (2, "trying to determine file info of input file '%s'", reader->name);
  Info res = FORMULA;
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
  msg (2, "assuming %s file info after peeking at %ld characters",
    (res==AIGER ? "AIGER" : (res==DIMACS ? "DIMACS" : "formula")), count);
  reader->info = res;
  return res;
}
