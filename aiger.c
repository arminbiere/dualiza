#include "headers.h"

typedef struct Aiger Aiger;

struct Aiger {
  Reader * reader;
  Symbols * symbols;
  unsigned max_index;
  unsigned num_inputs;
  unsigned num_ands;
  Gate * gates;
};

static unsigned parse_header_number (Reader * r, int expect_space) {
  Char ch = next_char (r);
  if (!isdigit (ch.code))
    parse_error (r, ch, "expected digit");
  unsigned res = ch.code - '0';
  ch = next_char (r);
  if (!res && ch.code == '0')
    parse_error (r, ch, "invalid number starting with two zeroes");
  while (isdigit (ch.code)) {
    if (UINT_MAX/10 < res) parse_error (r, ch, "number way too large");
    res *= 10;
    const int digit = ch.code - '0';
    if (UINT_MAX - digit < res) parse_error (r, ch, "number too large");
    res += digit;
    ch = next_char (r);
  }
  if (expect_space && ch.code != ' ')
    parse_error (r, ch, "expected space after '%u'", res);
  if (!expect_space && ch.code != '\n')
    parse_error (r, ch, "expected new line after '%u'", res);
  return res;
}

Circuit * parse_aiger (Reader * r, Symbols * t) {
  unsigned M, I, L, O, A;
  Char ch = next_char (r);
  if (ch.code != 'a') parse_error (r, ch, "expected 'a'");
  ch = next_char (r);
  int binary = 0;
  const char * fmtstr = 0;
  if (ch.code == 'i') binary = 1, fmtstr = "aig";
  else if (ch.code == 'a') binary = 0, fmtstr = "aag";
  else parse_error (r, ch, "expected 'a' or 'i' after 'a'");
  ch = next_char (r);
  if (ch.code != 'g')
    parse_error (r, ch, "expected 'g' after '%s'", fmtstr);
  if ((ch = next_char (r)).code != ' ')
    parse_error (r, ch, "expected space 'g' after '%s'", fmtstr);
  M = parse_header_number (r, 1);
  I = parse_header_number (r, 1);
  L = parse_header_number (r, 1);
  O = parse_header_number (r, 1);
  A = parse_header_number (r, 0);
  Circuit * res = new_circuit ();
  return res;
}
