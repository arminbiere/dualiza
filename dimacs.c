#include "headers.h"


Circuit * parse_dimacs (Reader * reader, Symbols * symbols) {
  Circuit * res = new_circuit ();
  Char ch;
  for (;;) {
    ch = next_char (reader);
    if (ch.code == 'c') {
      while ((ch = next_char (reader)).code != '\n')
	if (ch.code == EOF)
	  parse_error (reader, ch,
	    "unexpected end-of-file in header comment");
    }
    if (ch.code != 'p')
      parse_error (reader, ch, "expected 'p' or 'c'");
    if ((ch = next_char (reader)).code != ' ')
      parse_error (reader, ch, "expected ' ' after 'p'");
    if ((ch = next_char (reader)).code != 'c')
      parse_error (reader, ch, "expected 'c' after 'p '");
    if ((ch = next_char (reader)).code != 'n')
      parse_error (reader, ch, "expected 'n' after 'p c'");
    if ((ch = next_char (reader)).code != 'f')
      parse_error (reader, ch, "expected 'f' after 'p cn'");
    if ((ch = next_char (reader)).code != ' ')
      parse_error (reader, ch, "expected ' ' after 'p cnf'");
  }
  return res;
}
