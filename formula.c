#include "headers.h"

static Gate * parse_formula_expr (Reader *, Circuit *);

static Gate * parse_formula_basic (Reader * r, Circuit * c) {
  int ch = next_non_white_space_char (r);
  Gate * res = 0;
  if (ch == EOF) parse_error (r, "unexpected end-of-file");
  else if (ch == '(') {
    res = parse_formula_expr (r, c);
    ch = next_non_white_space_char (r);
  }
  return res;
}

static Gate * parse_formula_and (Reader * r, Circuit * c) {
  Gate * l = parse_formula_basic (r, c);
}

static Gate * parse_formula_xor (Reader * r, Circuit * c) {
  Gate * l = parse_formula_and (r, c);
}

static Gate * parse_formula_or (Reader * r, Circuit * c) {
  Gate * l = parse_formula_xor (r, c);
}

static Gate * parse_formula_ite (Reader * r, Circuit * c) {
  Gate * l = parse_formula_or (r, c);
}

static Gate * parse_formula_xnor (Reader * r, Circuit * c) {
  Gate * l = parse_formula_ite (r, c);
}

static Gate * parse_formula_expr (Reader * r, Circuit * c) {
  return parse_formula_xnor (r, c);
}

Circuit * parse_formula (const char * name, FILE * file) {
  Circuit * c = new_circuit ();
  Reader * r = new_reader (name, file);
  Gate * o = parse_formula_expr (r, c);
  delete_reader (r);
  connect_output (c, o);
  return c;
}
