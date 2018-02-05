#include "headers.h"

typedef struct Aiger Aiger;

struct Aiger {
  Circuit * circuit;
  Reader * reader;
  Symbols * symbols;
  unsigned max_index;
  unsigned num_inputs;
  unsigned num_ands;
  unsigned * inputs;
  Gate ** gates;
};

static Aiger *
new_aiger (Circuit * c,
           Reader * r, Symbols * t,
	   unsigned M, unsigned I, unsigned A) {
  assert (M == I + A);
  Aiger * res;
  NEW (res);
  res->circuit = c;
  res->reader = r;
  res->symbols = t;
  res->max_index = M;
  res->num_inputs = I;
  res->num_ands = A;
  ALLOC (res->inputs, I);
  for (unsigned i = 0; i < I; i++) res->inputs[i] = UINT_MAX;
  ALLOC (res->gates, M + 1);
  return res;
}

static void delete_aiger (Aiger * aiger) {
  DEALLOC (aiger->gates, aiger->max_index + 1);
  DEALLOC (aiger->inputs, aiger->num_inputs);
  DELETE (aiger);
}
 
static unsigned
parse_aiger_ascii_number (Reader * r, int expect_space, Char * chptr) {
  Char ch = next_char (r);
  if (chptr) *chptr = ch;
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

static void setup_aiger_symbol_table (Aiger * aiger) {
  for (unsigned i = 0; i < aiger->num_inputs; i++) {
    unsigned input = aiger->inputs[i];
    assert (1 < input);
    assert (!(input & 1));
    assert (input != UINT_MAX);
    assert (input <= 2*aiger->max_index);
    char name[32];
    sprintf (name, "i%u", i);
    Gate * g = aiger->gates[input/2];
    assert (g);
    assert (g->input == i);
    assert (g->idx == i);
    assert (PEEK (aiger->circuit->inputs, i) == g);
    Symbol * symbol = find_or_create_symbol (aiger->symbols, name);
    assert (!g->symbol);
    g->symbol = symbol;
    LOG (
"AIGER input %u literal %u connected to input gate %d with symbol '%s'",
        i, input, g->input, symbol->name);
  }
}

static void parse_ascii_aiger (Aiger * aiger) {
  for (unsigned i = 0; i < aiger->num_inputs; i++) {
    Char ch;
    unsigned input = parse_aiger_ascii_number (aiger->reader, 0, &ch);
    if (input < 2)
      parse_error (aiger->reader, ch, "invalid %u too small", input);
    if (input > 2*aiger->max_index)
      parse_error (aiger->reader, ch, "input %u too large", input);
    if (input & 1)
      parse_error (aiger->reader, ch, "invalid odd input %u", input);
    aiger->inputs[i] = input;
    if (aiger->gates[input/2])
      parse_error (aiger->reader, ch, "input %u defined twice", input);
    aiger->gates[input/2] = new_input_gate (aiger->circuit);
  }
  setup_aiger_symbol_table (aiger);
}

static void parse_binary_aiger (Aiger * aiger) {
  for (unsigned i = 0; i < aiger->num_inputs; i++) {
    assert (aiger->inputs[i] == UINT_MAX);
    unsigned input = 2*(i + 1);
    aiger->inputs[i] = input;
    assert (!aiger->gates[input/2]);
    aiger->gates[input/2] = new_input_gate (aiger->circuit);
  }
  setup_aiger_symbol_table (aiger);
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
  M = parse_aiger_ascii_number (r, 1, 0);
  I = parse_aiger_ascii_number (r, 1, 0);
  L = parse_aiger_ascii_number (r, 1, 0);
  O = parse_aiger_ascii_number (r, 1, 0);
  A = parse_aiger_ascii_number (r, 0, 0);
  if (L) parse_error (r, ch, "can not handle latches");
  if (UINT_MAX/2 < M)
    parse_error (r, ch, "maximum variable index %u too large", M);
  if (O != 1)
    parse_error (r, ch, "expected exactly one output but got %u", O);
  if (I > M || A > M || M - I != A)
    parse_error (r, ch,
      "invalid header M I L O A = %u %u %u %u %u",
      M, I, L, O, A);
  msg (1, "parsed M I L O A header %u %u %u %u %u", M, I, L, O, A);
  Circuit * res = new_circuit ();
  Aiger * aiger = new_aiger (res, r, t, M, I, A);
  if (binary) parse_binary_aiger (aiger);
  else parse_ascii_aiger (aiger);
  delete_aiger (aiger);
  return res;
}
