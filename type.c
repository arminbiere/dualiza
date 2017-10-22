#include "headers.h"

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
    (res==AIGER ? "AIGER" : (res==DIMACS ? "DIMACS" : "FORMULA")), count);
  return res;
}
