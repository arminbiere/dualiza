#include "headers.h"

extern int sat_competition_mode;

static int is_relevant_variable_string (const char * s) {
  if (!s) return 0;
  const char * p = s;
  int ch;
  for (;;) {
    while ((ch = *p++) == ' ' || ch == '\t')
      ;
    if (!isdigit (ch)) return 0;
    while (isdigit (ch = *p++))
      ;
    if (!ch) return 1;
    if (ch != ',') return 0;
  }
}

Circuit * parse_dimacs (Reader * r, Symbols * symbols,
                        IntStack ** relevant_ptr) {
  assert (relevant_ptr);
  Circuit * res = new_circuit ();
  CharStack comment;
  INIT (comment);
  Coo ch;
  int parsed_relevant_variables = 0;
  IntStack * relevant = 0;
  CooStack relevant_coos;
  INIT (relevant_coos);
  while ((ch = next_char (r)).code == 'c') {
    Coo comment_coo = ch;
    while ((ch = next_char (r)).code != '\n') {
      if (ch.code == EOF)
	parse_error (r, ch,
	  "unexpected end-of-file in header comment");
      PUSH (comment, ch.code);
    }
    if (options.project && 
        is_relevant_variable_string (comment.start)) {
      PUSH (comment, 0);
      LOG ("parsing comment for relevant variables: %s", comment.start);
      int dh;
      for (const char * p = comment.start; (dh = *p); p++) {
	if (dh == ' ') continue;
	if (isdigit (dh)) {
	  Coo relevant_coo = comment_coo;
	  relevant_coo.column += p - comment.start + 1;
	  int idx = dh - '0';
	  if (!idx)
	    parse_error (r, relevant_coo, "invalid relevant index");
	  while (isdigit (dh = p[1])) {
	    if (INT_MAX/10 < idx)
	      parse_error (r, relevant_coo,
	        "really too large relevant index");
	    else {
	      idx *= 10;
	      const int digit = dh - '0';
	      if (INT_MAX - digit < idx)
		parse_error (r, relevant_coo, "too large relevant index");
	      else idx += digit, p++;
	    }
	  }
	  if (dh == ',' || !dh) {
	    if (dh) p++;
	    LOG ("found relevant variable index %d", idx);
	    if (!relevant) {
	      NEW (relevant);
	      INIT (*relevant);
	      *relevant_ptr = relevant;
	    }
	    PUSH (*relevant, idx);
	    PUSH (relevant_coos, relevant_coo);
	  } else {
	    assert (p[1] == dh);
	    Coo after_dh_coo = comment_coo;
	    after_dh_coo.column += p - comment.start + 2;
	    parse_error (r, after_dh_coo,
	      "expected ',' or digit at character %s",
	      make_character_printable_as_string (dh));
	  }
	} else {
	  assert (*p == dh);
	  Coo dh_coo = comment_coo;
	  dh_coo.column += p - comment.start + 1;
	  parse_error (r, dh_coo,
	    "expected digit at character %s",
	    make_character_printable_as_string (dh));
	}
      }
      const size_t n = COUNT (*relevant);
      assert (n > 0);
      msg (2, "found relevant variable line with %zd variables", n);
      parsed_relevant_variables = 1;
    }
    CLEAR (comment);
  }
  RELEASE (comment);
  if (ch.code != 'p')
    parse_error_at (r, ch, "expected 'p' or 'c'");
  if ((ch = next_char (r)).code != ' ')
    parse_error_at (r, ch, "expected space after 'p'");
  if ((ch = next_char (r)).code != 'c')
    parse_error_at (r, ch, "expected 'c' after 'p '");
  if ((ch = next_char (r)).code != 'n')
    parse_error_at (r, ch, "expected 'n' after 'p c'");
  if ((ch = next_char (r)).code != 'f')
    parse_error_at (r, ch, "expected 'f' after 'p cn'");
  if ((ch = next_char (r)).code != ' ')
    parse_error_at (r, ch, "expected space after 'p cnf'");
  if (!isdigit ((ch = next_char (r)).code))
    parse_error_at (r, ch, "expected digit after 'p cnf '");
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
    parse_error_at (r, ch, "expected space after 'p cnf %d'", s);
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

    char * seen;
    ALLOC (seen, s + 1);

    assert (relevant);
    assert (!EMPTY (*relevant));

    const size_t n = COUNT (*relevant);
    size_t j = 0;

    for (size_t i = 0; i < n; i++) {
      const int idx = PEEK (*relevant, i);
      assert (0 < idx);
      Coo first = PEEK (relevant_coos, i);
      if (idx > s)
	parse_error (r, first, "relevant index '%d' too large'", idx);
      if (seen[idx]) LOG ("ignoring duplicated relevant index '%d'", idx);
      else {
	POKE (*relevant, j, idx);
	seen[idx] = 1;
	j++;
      }
    }
    RESIZE (*relevant, j);

    msg (1, "parsed %zd relevant variables", j);

    DEALLOC (seen, s + 1);

    int cmp (const void * p, const void * q) {
      return *(int*)p - *(int*)q;
      int a = * (int *) p, b = * (int *) q;
      return a - b;
    }

    qsort (relevant->start, j, sizeof (int), cmp);
  }
  RELEASE (relevant_coos);

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
	parse_error_at (r, ch, "expected digit after '-'");
      if (ch.code == '0')
	parse_error_at (r, ch, "expected non-zero digit after '-'");
      sign = 1;
    } else if (!isdigit (ch.code))
      parse_error_at (r, ch, "expected digit or '-'");
    Coo start = ch;
    int i = ch.code - '0';
    while (isdigit ((ch = next_char (r)).code)) {
      if (!i) parse_error (r, start, "invalid variable index number");
      if (INT_MAX/10 < i)
	parse_error (r, start, "really too large variable index");
      i *= 10;
      const int digit = ch.code - '0';
      if (INT_MAX - digit < i)
	parse_error (r, start, "too large variable index");
      i += digit;
    }
    if (i > s)
      parse_error (r, start, "maximum variable index %d exceeded", s);
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
	parse_error_at (r, ch, "unexpected character after literal");
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
