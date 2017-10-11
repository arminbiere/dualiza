#include "headers.h"

static Gate * parse_formula_expr (Reader *, Circuit *);

static Gate * parse_formula_basic (Reader * r, Circuit * c) {
  int ch = next_non_white_space_char (r);
  Gate * res = 0;
  if (ch == EOF) parse_error (r, "unexpected end-of-file");
  else if (ch == '(') {
    res = parse_formula_expr (r, c);
    ch = next_non_white_space_char (r);
    if (ch != ')') parse_error (r, "expected ')'");
  } else if (ch == '!') {
    res = parse_formula_basic (r, c);
    res = NOT (res);
  } else if (ch == '1') res = NOT (new_false_gate (c));
  else if (ch == '0') res = new_false_gate (c);
  else if (ch == 'x') {
    ch = next_char (r);
    if (!isdigit (ch)) parse_error (r, "expected digit after 'x'");
    else {
      int input = ch - '0';
      while (isdigit (ch = next_char (r))) {
	if (!input)
	  parse_error (r, "invalid input index starts with '0'");
	if (input > INT_MAX/10)
	  parse_error (r, "input index really too large");
	input *= 10;
	int digit = ch - '0';
	if (input > INT_MAX - digit)
	  parse_error (r, "input index too large");
	input += digit;
      }
      prev_char (r, ch);
      res = new_input_gate (c, input);
    }
  } else parse_error (r, "expected basic expression");
  return res;
}

static Gate * parse_formula_and (Reader * r, Circuit * c) {
  Gate * res = parse_formula_basic (r, c), * and = 0;
  for (;;) {
    int ch = next_non_white_space_char (r);
    if (ch != '&') { prev_char (r, ch); return res; }
    Gate * tmp = parse_formula_basic (r, c);
    if (!and) {
      and = new_and_gate (c);
      connect_gates (res, and);
      res = and;
    }
    connect_gates (tmp, and);
  } 
}

static Gate * parse_formula_xor (Reader * r, Circuit * c) {
  Gate * first = parse_formula_and (r, c);
  int ch = next_non_white_space_char (r);
  if (ch != '^') { prev_char (r, ch); return first; }
  Gate * second = parse_formula_and (r, c);
  Gate * xor = new_xor_gate (c);
  connect_gates (first, xor);
  connect_gates (second, xor);
  return xor;
}

static Gate * parse_formula_or (Reader * r, Circuit * c) {
  Gate * res = parse_formula_xor (r, c), * or = 0;
  for (;;) {
    int ch = next_non_white_space_char (r);
    if (ch != '|') { prev_char (r, ch); return res; }
    Gate * tmp = parse_formula_xor (r, c);
    if (!or) {
      or = new_or_gate (c);
      connect_gates (res, or);
      res = or;
    }
    connect_gates (tmp, or);
  } 
}

static Gate * parse_formula_ite (Reader * r, Circuit * c) {
  Gate * cond = parse_formula_or (r, c);
  int ch = next_non_white_space_char (r);
  if (ch != '?') { prev_char (r, ch); return cond; }
  Gate * t = parse_formula_or (r, c);
  ch = next_non_white_space_char (r);
  if (ch != ':') parse_error (r, "expected ':'");
  Gate * e = parse_formula_or (r, c);
  Gate * ite = new_ite_gate (c);
  connect_gates (cond, ite);
  connect_gates (t, ite);
  connect_gates (e, ite);
  return ite;
}

static Gate * parse_formula_xnor (Reader * r, Circuit * c) {
  Gate * first = parse_formula_ite (r, c);
  int ch = next_non_white_space_char (r);
  if (ch != '=') { prev_char (r, ch); return first; }
  Gate * second = parse_formula_ite (r, c);
  Gate * xnor = new_xnor_gate (c);
  connect_gates (first, xnor);
  connect_gates (second, xnor);
  return xnor;
}

static Gate * parse_formula_expr (Reader * r, Circuit * c) {
  return parse_formula_xnor (r, c);
}

Circuit * parse_formula (const char * name, FILE * file) {
  Circuit * c = new_circuit ();
  Reader * r = new_reader (name, file);
  Gate * o = parse_formula_expr (r, c);
  int ch = next_non_white_space_char (r);
  if (ch != EOF)
    parse_error (r, "expected end-of-file after expression");
  delete_reader (r);
  connect_output (c, o);
  return c;
}
