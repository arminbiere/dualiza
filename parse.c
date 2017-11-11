#include "headers.h"

typedef struct Parser Parser;

struct Parser {
  Reader * reader;
  Symbols * symbols;
  Circuit * circuit;
};

static Gate * parse_expr (Parser *);

static int is_symbol_start (int ch) {
  if ('a' <= ch && ch <= 'z') return 1;
  if ('A' <= ch && ch <= 'Z') return 1;
  if (ch == '_') return 1;
  return 0;
}

static int is_symbol_character (int ch) {
  if ('0' <= ch && ch <= '9') return 1;
  return is_symbol_start (ch);
}

static Gate * parse_basic (Parser * parser) {
  Char ch = next_non_white_space_char (parser->reader);
  Gate * res = 0;
  if (ch.code == EOF)
    parse_error (parser->reader, "unexpected end-of-file");
  else if (ch.code == '(') {
    res = parse_expr (parser);
    ch = next_non_white_space_char (parser->reader);
    if (ch.code != ')') parse_error (parser->reader, "expected ')'");
  } else if (ch.code == '!') {
    res = parse_basic (parser);
    res = NOT (res);
  } else if (ch.code == '1') res = NOT (new_false_gate (parser->circuit));
  else if (ch.code == '0') res = new_false_gate (parser->circuit);
  else if (is_symbol_start (ch.code)) {
    Coo start = ch.coo, end = ch.coo;
    assert (EMPTY (parser->reader->symbol));
    PUSH (parser->reader->symbol, ch.code);
    while (is_symbol_character ((ch = next_char (parser->reader)).code)) {
      PUSH (parser->reader->symbol, ch.code);
      end = ch.coo;
    }
    (void) start; // TODO use it ...
    (void) end; // TODO use it ...
    prev_char (parser->reader, ch);
    PUSH (parser->reader->symbol, 0);
    const char * name = parser->reader->symbol.start;
    Symbol * s = find_or_create_symbol (parser->symbols, name);
    CLEAR (parser->reader->symbol);
    if (!(res = s->gate)) {
      res = s->gate = new_input_gate (parser->circuit);
      res->symbol = s;
      LOG (
        "formula input %d connected to circuit gate %d with symbol '%s'",
        res->input, res->idx, s->name);
    }
  } else parse_error (parser->reader, "expected basic expression");
  return res;
}

static Gate * parse_and (Parser * parser) {
  Gate * res = parse_basic (parser), * and = 0;
  for (;;) {
    Char ch = next_non_white_space_char (parser->reader);
    if (ch.code != '&') { prev_char (parser->reader, ch); return res; }
    Gate * tmp = parse_basic (parser);
    if (!and) {
      and = new_and_gate (parser->circuit);
      connect_gates (res, and);
      res = and;
    }
    connect_gates (tmp, and);
  } 
}

static Gate * parse_xor (Parser * parser) {
  Gate * res = parse_and (parser), * xor = 0;
  for (;;) {
    Char ch = next_non_white_space_char (parser->reader);
    if (ch.code != '^') { prev_char (parser->reader, ch); return res; }
    Gate * tmp = parse_and (parser);
    if (!xor) {
      xor = new_xor_gate (parser->circuit);
      connect_gates (res, xor);
      res = xor;
    }
    connect_gates (tmp, xor);
  } 
}

static Gate * parse_or (Parser * parser) {
  Gate * res = parse_xor (parser), * or = 0;
  for (;;) {
    Char ch = next_non_white_space_char (parser->reader);
    if (ch.code != '|') { prev_char (parser->reader, ch); return res; }
    Gate * tmp = parse_xor (parser);
    if (!or) {
      or = new_or_gate (parser->circuit);
      connect_gates (res, or);
      res = or;
    }
    connect_gates (tmp, or);
  } 
}

static Gate * parse_ite (Parser * parser) {
  Gate * cond = parse_or (parser);
  Char ch = next_non_white_space_char (parser->reader);
  if (ch.code != '?') { prev_char (parser->reader, ch); return cond; }
  Gate * pos = parse_or (parser);
  ch = next_non_white_space_char (parser->reader);
  if (ch.code != ':') parse_error (parser->reader, "expected ':'");
  Gate * neg = parse_or (parser);
  Gate * ite = new_ite_gate (parser->circuit);
  connect_gates (cond, ite);
  connect_gates (pos, ite);
  connect_gates (neg, ite);
  return ite;
}

static Gate * parse_equal (Parser * parser) {
  Gate * res = parse_ite (parser), * xnor = 0;
  for (;;) {
    Char ch = next_non_white_space_char (parser->reader);
    if (ch.code != '=') { prev_char (parser->reader, ch); return res; }
    Gate * tmp = parse_ite (parser);
    if (!xnor) {
      xnor = new_xnor_gate (parser->circuit);
      connect_gates (res, xnor);
      res = xnor;
    }
    connect_gates (tmp, xnor);
  } 
}

#if 1

static Gate * parse_expr (Parser * parser) {
  return parse_equal (parser);
}

#else

static Gate * parse_implies (Parser * parser) {
  Gate * antecendent = parse_equal (parser);
  Char ch = next_non_white_space_char (parser->reader);
  if (ch.code != '>') { prev_char (parser->reader, ch); return antecendent; }
  Gate * conclusion = parse_equal (parser);
  Gate * or = new_or_gate (parser->circuit);
  connect_gates (NOT (antecendent), or);
  connect_gates (conclusion, or);
  return or;
}

static Gate * parse_expr (Parser * parser) {
  return parse_implies (parser);
}

#endif

Circuit * parse_circuit (Reader * reader, Symbols * symbols) {
  Parser parser;
  parser.reader = reader;
  parser.symbols = symbols;
  parser.circuit = new_circuit ();
  Gate * output = parse_expr (&parser);
  Char ch = next_non_white_space_char (reader);
  if (ch.code != EOF)
    parse_error (reader, "expected end-of-file after expression");
  connect_output (parser.circuit, output);
  return parser.circuit;
}
