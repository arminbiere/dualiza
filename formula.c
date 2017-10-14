#include "headers.h"

static Gate * parse_formula_expr (Reader *, Symbols * t, Circuit *);

static int is_symbol_start (int ch) {
  if ('a' <= ch && ch <= 'z') return 1;
  if ('A' <= ch && ch <= 'Z') return 1;
  if (ch == '_') return 1;
  return 0;
}

static int is_symbol_char (int ch) {
  if ('0' <= ch && ch <= '9') return 1;
  return is_symbol_start (ch);
}

static Gate * parse_formula_basic (Reader * r, Symbols * t, Circuit * c) {
  int ch = next_non_white_space_char (r);
  Gate * res = 0;
  if (ch == EOF) parse_error (r, "unexpected end-of-file");
  else if (ch == '(') {
    res = parse_formula_expr (r, t, c);
    ch = next_non_white_space_char (r);
    if (ch != ')') parse_error (r, "expected ')'");
  } else if (ch == '!') {
    res = parse_formula_basic (r, t, c);
    res = NOT (res);
  } else if (ch == '1') res = NOT (new_false_gate (c));
  else if (ch == '0') res = new_false_gate (c);
  else if (is_symbol_start (ch)) {
    assert (EMPTY (r->symbol));
    PUSH (r->symbol, ch);
    while (is_symbol_char (ch = next_char (r)))
      PUSH (r->symbol, ch);
    prev_char (r, ch);
    PUSH (r->symbol, 0);
    Symbol * s = find_or_create_symbol (t, r->symbol.start);
    CLEAR (r->symbol);
    if (!(res = s->gate)) {
      res = s->gate = new_input_gate (c);
      res->symbol = s;
    }
  } else parse_error (r, "expected basic expression");
  return res;
}

static Gate * parse_formula_and (Reader * r, Symbols * t, Circuit * c) {
  Gate * res = parse_formula_basic (r, t, c), * and = 0;
  for (;;) {
    int ch = next_non_white_space_char (r);
    if (ch != '&') { prev_char (r, ch); return res; }
    Gate * tmp = parse_formula_basic (r, t, c);
    if (!and) {
      and = new_and_gate (c);
      connect_gates (res, and);
      res = and;
    }
    connect_gates (tmp, and);
  } 
}

static Gate * parse_formula_xor (Reader * r, Symbols * t, Circuit * c) {
  Gate * first = parse_formula_and (r, t, c);
  int ch = next_non_white_space_char (r);
  if (ch != '^') { prev_char (r, ch); return first; }
  Gate * second = parse_formula_and (r, t, c);
  Gate * xor = new_xor_gate (c);
  connect_gates (first, xor);
  connect_gates (second, xor);
  return xor;
}

static Gate * parse_formula_or (Reader * r, Symbols * t, Circuit * c) {
  Gate * res = parse_formula_xor (r, t, c), * or = 0;
  for (;;) {
    int ch = next_non_white_space_char (r);
    if (ch != '|') { prev_char (r, ch); return res; }
    Gate * tmp = parse_formula_xor (r, t, c);
    if (!or) {
      or = new_or_gate (c);
      connect_gates (res, or);
      res = or;
    }
    connect_gates (tmp, or);
  } 
}

static Gate * parse_formula_ite (Reader * r, Symbols * t, Circuit * c) {
  Gate * cond = parse_formula_or (r, t, c);
  int ch = next_non_white_space_char (r);
  if (ch != '?') { prev_char (r, ch); return cond; }
  Gate * pos = parse_formula_or (r, t, c);
  ch = next_non_white_space_char (r);
  if (ch != ':') parse_error (r, "expected ':'");
  Gate * neg = parse_formula_or (r, t, c);
  Gate * ite = new_ite_gate (c);
  connect_gates (cond, ite);
  connect_gates (pos, ite);
  connect_gates (neg, ite);
  return ite;
}

static Gate * parse_formula_xnor (Reader * r, Symbols * t, Circuit * c) {
  Gate * first = parse_formula_ite (r, t, c);
  int ch = next_non_white_space_char (r);
  if (ch != '=') { prev_char (r, ch); return first; }
  Gate * second = parse_formula_ite (r, t, c);
  Gate * xnor = new_xnor_gate (c);
  connect_gates (first, xnor);
  connect_gates (second, xnor);
  return xnor;
}

static Gate * parse_formula_expr (Reader * r, Symbols * t, Circuit * c) {
  return parse_formula_xnor (r, t, c);
}

Circuit * parse_formula (Reader * r, Symbols * t) {
  Circuit * c = new_circuit ();
  assert (!r->comment);
  r->comment = '-';
  Gate * o = parse_formula_expr (r, t, c);
  int ch = next_non_white_space_char (r);
  if (ch != EOF) parse_error (r, "expected end-of-file after expression");
  connect_output (c, o);
  return c;
}
