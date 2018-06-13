#include "headers.h"

extern int sat_competition_mode;

Circuit * parse_dimacs (Reader * r, Symbols * symbols, IntStack * relevant) {
  Circuit * res = new_circuit ();
  CharStack comment;
  INIT (comment);
  Char ch;
  int parsed_relevant_variables = 0;
  while ((ch = next_char (r)).code == 'c') {
    while ((ch = next_char (r)).code != '\n') {
      if (ch.code == EOF)
	parse_error (r, ch,
	  "unexpected end-of-file in header comment");
      if (parsed_relevant_variables) continue;
      if (!EMPTY (comment) || ch.code != ' ') PUSH (comment, ch.code);
    }
    if (options.project &&
        !parsed_relevant_variables &&
	!EMPTY (comment)) {
      PUSH (comment, 0);
      LOG ("parsing comment for relevant variables: %s", comment.start);
      const char * err = 0;
      int dh;
      for (const char * p = comment.start; !err && (dh = *p); p++) {
	if (isdigit (dh)) {
	  int idx = dh - '0';
	  while (!err && isdigit (dh = p[1])) {
	    if (!idx) err = "invalid index";
	    else if (INT_MAX/10 < idx) err = "really too large index";
	    else {
	      idx *= 10;
	      const int digit = dh - '0';
	      if (INT_MAX - digit < idx) err = "too large index";
	      else idx += digit, p++;
	    }
	  }
	  if (err) break;
	  if (dh == ',' || !dh) {
	    if (dh) p++;
	    LOG ("found relevant variable index %d", idx);
	    PUSH (*relevant, idx);
	  } else err = "expected ',' or digit";
	} else err = "expected digit";
      }
      if (!err) {
	assert (!EMPTY (*relevant));
	int cmp (const void * p, const void * q) {
	  int l = * (int*) p, k = * (int*) q, res = abs (l) - abs (k);
	  return res ? res : l - k;
	}
	const size_t n = COUNT (*relevant);
	qsort (relevant->start, n, sizeof (int), cmp);
	for (size_t i = 1; !err && i < n; i++) {
	  int prev = PEEK (*relevant, i-1);
	  int current = PEEK (*relevant, i);
	  if (prev == current) err = "duplicated variable";
	}
      }
      if (err) {
	if (EMPTY (*relevant))
	  LOG ("parse error relevant variable line: %s", err);
	else msg (2, "parse error relevant variable line: %s", err);
	CLEAR (*relevant);
      } else {
	const size_t n = COUNT (*relevant);
	assert (n > 0);
	msg (2, "found relevant variable line with %zd variables", n);
	parsed_relevant_variables = 1;
      }
    }
    CLEAR (comment);
  }
  RELEASE (comment);
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
  if (parsed_relevant_variables) {
    assert (!EMPTY (*relevant));
    const size_t n = COUNT (*relevant);
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
      const int idx = PEEK (*relevant, i);
      if (idx > s) msg (0, "ignoring too large relevant index %d", idx);
      else {
	POKE (*relevant, j, idx);
	j++;
      }
    }
    relevant->top = relevant->start + j;
    msg (1, "found %zd relevant variables", j);
  } else {
    LOG ("all variables relevant since none explicitly specified");
    for (int idx = 1; idx <= s; idx++)
      PUSH (*relevant, idx);
  }
  LOG ("connecting %d input gates to DIMACS variables", s);
  for (int i = 0; i < s; i++) {
    char name[32];
    sprintf (name, "%s%d", (sat_competition_mode ? "" : "x"), i + 1);
    Gate * g = new_input_gate (res);
    assert (g->input == i);
    assert (g->idx == i);
    assert (PEEK (res->inputs, i) == g);
    Symbol * symbol = find_or_create_symbol (symbols, name);
    g->symbol = symbol;
    LOG (
      "DIMACS variable %d connected to input gate %d with symbol '%s'",
      i + 1, g->input, symbol->name);
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
