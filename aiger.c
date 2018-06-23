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

static int is_valid_aiger_literal (Aiger * aiger, unsigned lit) {
  return lit <= 2*aiger->max_index + 1;
}

static Gate * aiger_literal_to_gate (Aiger * aiger, unsigned lit) {
  assert (is_valid_aiger_literal (aiger, lit));
  if (lit < 2) {
    if (!aiger->gates[0]) {
      LOG ("need AIGER constant too");
      aiger->gates[0] = new_false_gate (aiger->circuit);
    }
  }
  Gate * res = aiger->gates[lit/2];
  if (res && (lit & 1)) res = NOT (res);
  return res;
}
 
static unsigned
parse_aiger_ascii_number (Reader * r, int expect_space, Coo * chptr) {
  Coo ch = next_char (r);
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

static unsigned
parse_aiger_ascii_literal (Aiger * aiger, int expect_space, Coo * chptr) {
  Coo ch;
  unsigned res =
    parse_aiger_ascii_number (aiger->reader, expect_space, &ch);
  if (!is_valid_aiger_literal (aiger, res))
    parse_error (aiger->reader, ch, "invalid literal %u (too large)", res);
  if (chptr) *chptr = ch;
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
  Reader * r = aiger->reader;
  Coo ch;
  for (unsigned i = 0; i < aiger->num_inputs; i++) {
    unsigned input = parse_aiger_ascii_literal (aiger, 0, &ch);
    if (input < 2)
      parse_error (r, ch, "input literal %u too small", input);
    if (input & 1)
      parse_error (r, ch, "invalid odd input literal %u", input);
    aiger->inputs[i] = input;
    if (aiger->gates[input/2])
      parse_error (r, ch, "input %u defined twice", input);
    aiger->gates[input/2] = new_input_gate (aiger->circuit);
  }
  setup_aiger_symbol_table (aiger);
  unsigned output = parse_aiger_ascii_literal (aiger, 0, &ch);
  Coo output_ch = ch;
  LOG ("AIGER output literal %u", output);
  for (unsigned i = 0; i < aiger->num_ands; i++) {
    unsigned lhs = parse_aiger_ascii_literal (aiger, 1, &ch);
    if (lhs < 2)
      parse_error (r, ch, "AND literal %u too small", lhs);
    if (lhs & 1)
      parse_error (r, ch, "invalid odd AND literal %u", lhs);
    if (aiger->gates[lhs/2])
      parse_error (r, ch, "AND literal %u already defined", lhs);
    unsigned rhs0 = parse_aiger_ascii_literal (aiger, 1, &ch);
    if (rhs0 >= lhs)
      parse_error (r, ch, "AND gate argument %u >= %u", rhs0, lhs);
    Gate * g0 = aiger_literal_to_gate (aiger, rhs0);
    if (!g0)
      parse_error (r, ch, "AND gate argument %u undefined", rhs0);
    unsigned rhs1 = parse_aiger_ascii_literal (aiger, 0, &ch);
    if (rhs1 >= lhs)
      parse_error (r, ch, "AND gate argument %u >= %u", rhs1, lhs);
    Gate * g1 = aiger_literal_to_gate (aiger, rhs1);
    if (!g1)
      parse_error (r, ch, "AND gate argument %u undefined", rhs1);
    LOG ("parsed AIGER AND gate %u = %u & %u", lhs, rhs0, rhs1);
    Gate * and = new_and_gate (aiger->circuit);
    connect_gates (g0, and);
    connect_gates (g1, and);
    aiger->gates[lhs/2] = and;
  }
  Gate * output_gate = aiger_literal_to_gate (aiger, output);
  if (!output_gate)
    parse_error (r, output_ch, "output literal %u undefined", output);
  connect_output (aiger->circuit, output_gate);
}

static Coo parse_aiger_binary_char (Reader * reader) {
  Coo ch = next_char (reader);
  if (ch.code == EOF)
    parse_error (reader, ch,
      "unexpected end-of-literal while reading binary literal");
  return ch;
}

static unsigned
parse_aiger_binary_number (Reader * reader, Coo * chptr) {
  unsigned res = 0, i = 0;
  unsigned char uch;
  Coo ch;
  while ((uch = (ch = parse_aiger_binary_char (reader)).code) & 0x80) {
    if (i == 4 && uch > 15)
      parse_error (reader, ch, "invalid binary encoding");
    res |= (uch & 0x7f) << (7 * i++);
  }
  if (chptr) *chptr = ch;
  return res | (uch << (7*i));
}

static void parse_binary_aiger (Aiger * aiger) {
  Reader * r = aiger->reader;
  for (unsigned i = 0; i < aiger->num_inputs; i++) {
    assert (aiger->inputs[i] == UINT_MAX);
    unsigned input = 2*(i + 1);
    aiger->inputs[i] = input;
    assert (!aiger->gates[input/2]);
    aiger->gates[input/2] = new_input_gate (aiger->circuit);
  }
  setup_aiger_symbol_table (aiger);
  Coo ch;
  unsigned output = parse_aiger_ascii_literal (aiger, 0, &ch);
  assert (is_valid_aiger_literal (aiger, output));
  LOG ("AIGER output literal %u", output);
  for (unsigned i = 0; i < aiger->num_ands; i++) {
    unsigned lhs = 2*(aiger->num_inputs + i + 1);
    unsigned delta0 = parse_aiger_binary_number (r, &ch);
    if (!delta0 || lhs < delta0)
      parse_error (r, ch,
        "invalid zero delta encoding in first argument");
    unsigned rhs0 = lhs - delta0;
    Gate * g0 = aiger_literal_to_gate (aiger, rhs0);
    assert (g0);
    unsigned delta1 = parse_aiger_binary_number (r, &ch);
    if (rhs0 < delta1)
      parse_error (r, ch,
        "invalid zero delta encoding in first argument");
    unsigned rhs1 = rhs0 - delta1;
    Gate * g1 = aiger_literal_to_gate (aiger, rhs1);
    assert (g1);
    Gate * and = new_and_gate (aiger->circuit);
    connect_gates (g0, and);
    connect_gates (g1, and);
    aiger->gates[lhs/2] = and;
  }
  Gate * output_gate = aiger_literal_to_gate (aiger, output);
  assert (output_gate);
  connect_output (aiger->circuit, output_gate);
}

Circuit * parse_aiger (Reader * r, Symbols * t) {
  unsigned M, I, L, O, A;
  Coo ch = next_char (r);
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
