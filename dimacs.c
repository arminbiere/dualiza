#include "headers.h"

Circuit * parse_dimacs (Reader * r, Symbols * symbols) {
  Circuit * res = new_circuit ();
  Char ch;
  while ((ch = next_char (r)).code == 'c')
    while ((ch = next_char (r)).code != '\n')
      if (ch.code == EOF)
	parse_error (r, ch,
	  "unexpected end-of-file in header comment");
  if (ch.code != 'p')
    parse_error (r, ch, "expected 'p' or 'c'");
  if ((ch = next_char (r)).code != ' ')
    parse_error (r, ch, "expected space after 'p'");
  if ((ch = next_char (r)).code != 'c')
    parse_error (r, ch, "expected 'c' after 'p '");
  if ((ch = next_char (r)).code != 'n')
    parse_error (r, ch, "expected 'n' after 'p c'");
  if ((ch = next_char (r)).code != 'f')
    parse_error (r, ch, "expected 'f' after 'p cn'");
  if ((ch = next_char (r)).code != ' ')
    parse_error (r, ch, "expected space after 'p cnf'");
  if (!isdigit ((ch = next_char (r)).code))
    parse_error (r, ch, "expected digit after 'p cnf '");
  int s = ch.code - '0';
  while (isdigit ((ch = next_char (r)).code)) {
    if (!s) parse_error (r, ch, "invalid number of variables");
    if (INT_MAX/10 < s)
      parse_error (r, ch, "really too many variables specified");
    s *= 10;
    const int digit = ch.code - '0';
    if (INT_MAX - digit < s)
      parse_error (r, ch, "too many variables specified");
    s += digit;
  }
  LOG ("parsed number of variables %d in DIMACS header", s);
  if (ch.code != ' ')
    parse_error (r, ch, "expected space after 'p cnf %d'", s);
  if (!isdigit ((ch = next_char (r)).code))
    parse_error (r, ch, "expected digit after 'p cnf %d '", s);
  int t = ch.code - '0';
  while (isdigit ((ch = next_char (r)).code)) {
    if (!t) parse_error (r, ch, "invalid number of clauses");
    if (INT_MAX/10 < t)
      parse_error (r, ch, "really too many clauses specified");
    t *= 10;
    const int digit = ch.code - '0';
    if (INT_MAX - digit < t)
      parse_error (r, ch, "too many clauses specified");
    t += digit;
  }
  LOG ("parsed number of clauses %d in DIMACS header", t);
  while (ch.code != '\n')
    if (!is_space_character (ch.code))
      parse_error (r, ch,
	"non-space character before newline after 'p cnf %d %d'", s, t);
    else ch = next_char (r);
  msg (1, "parsed 'p cnf %d %d' header", s, t);
  LOG ("connecting %d input gates to DIMACS variables", s);
  for (int i = 0; i < s; i++) {
    char name[32];
    sprintf (name, "x%d", i + 1);
    Gate * g = new_input_gate (res);
    assert (g->input == i);
    assert (g->idx == i);
    assert (PEEK (res->inputs, i) == g);
    Symbol * s = find_or_create_symbol (symbols, name);
    g->symbol = s;
    LOG (
      "DIMACS variable %d connected to input gate %d with symbol '%s'",
      i + 1, g->input, s->name);
    assert (g->input == g->idx);
  }
  STACK (Gate *) clauses, clause;
  INIT (clauses); INIT (clause);
  ch = next_non_white_space_char (r);
  for (;;) {
    if (ch.code == EOF) {
      if (!EMPTY (clause))
	parse_error (r, ch,
	  "end-of-file without '0' after last clause");
      if (COUNT (clauses) == t - 1)
	parse_error (r, ch, "single clause missing");
      if (COUNT (clauses) < t)
	parse_error (r, ch,
	  "%ld clauses missing", (long)(t - COUNT (clauses)));
      assert (COUNT (clauses) == t);
      break;
    }
    int sign = 0;
    if (ch.code == '-') {
      ch = next_char (r);
      if (!isdigit (ch.code))
	parse_error (r, ch, "expected digit after '-'");
      if (ch.code == '0')
	parse_error (r, ch, "expected non-zero digit after '-'");
      sign = 1;
    } else if (!isdigit (ch.code))
      parse_error (r, ch, "expected digit or '-'");
    int i = ch.code - '0';
    while (isdigit ((ch = next_char (r)).code)) {
      if (!i) parse_error (r, ch, "invalid variable index number");
      if (INT_MAX/10 < i)
	parse_error (r, ch, "really too large variable index");
      i *= 10;
      const int digit = ch.code - '0';
      if (INT_MAX - digit < i)
	parse_error (r, ch, "too large variable index");
      i += digit;
    }
    if (i > s)
      parse_error (r, ch, "maximum variable index %d exceeded", s);
    assert (COUNT (clauses) <= t);
    if (COUNT (clauses) == t)
      parse_error (r, ch, "more clauses than specified");
#ifndef NLOG
    if (EMPTY (clause)) LOG ("start clause %ld", 1 + COUNT (clauses));
#endif
    if (i) {
      Gate * g = PEEK (res->inputs, i - 1);
      assert (g);
      if (sign) g = NOT (g);
      PUSH (clause, g);
    } else {
      Gate * g;
      if (EMPTY (clause)) g = new_false_gate (res);
      else if (COUNT (clause) == 1) g = PEEK (clause, 0);
      else {
	g = new_or_gate (res);
	for (Gate ** p = clause.start; p < clause.top; p++)
	  connect_gates (*p, g);
      }
      CLEAR (clause);
      PUSH (clauses, g);
#ifndef NLOG
      LOG ("end clause %ld", COUNT (clauses));
#endif
    }
    if (ch.code != EOF) {
      if (!is_space_character (ch.code))
	parse_error (r, ch, "unexpected character after literal");
      prev_char (r, ch);
      ch = next_non_white_space_char (r);
    }
  }
  RELEASE (clause);
  Gate * g;
  if (EMPTY (clauses)) g = NOT (new_false_gate (res));
  else if (COUNT (clauses) == 1) g = PEEK (clauses, 0);
  else {
    g = new_and_gate (res);
    for (Gate ** p = clauses.start; p < clauses.top; p++)
      connect_gates (*p, g);
  }
  RELEASE (clauses);
  connect_output (res, g);
  return res;
}
