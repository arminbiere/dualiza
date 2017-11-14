#include "headers.h"

typedef struct Encoder Encoder;

struct Encoder {
  Encoding * encoding;
  Circuit * circuit;
  CNF * cnf;
  int * map;
  int negative;
  STACK (int) clause, marks;
};

Encoding * new_encoding () {
  Encoding * res;
  NEW (res);
  return res;
}

void delete_encoding (Encoding * e) {
  RELEASE (*e);
  DELETE (e);
}

static void encode_input (Encoding * e, Gate * g, int idx) {
  assert (idx > 0);
  assert (g->op == INPUT);
  while (COUNT (*e) <= idx)
    PUSH (*e, 0);
  assert (!PEEK (*e, idx));
  POKE (*e, idx, g);
  LOG ("input %d gate %d encoded with literal %d",
    g->input, g->idx, idx);
}

#if 0
Gate * decode_literal (Encoding * e, int idx) {
  assert (0 < idx), assert (idx < COUNT (e->inputs));
  Gate * res  = e->inputs.start[idx];
  assert (res);
  return res;
}
#endif

void print_dimacs_encoding_to_file (Encoding * e, FILE * file) {
  const int num_encoded = COUNT (*e);
  for (int i = 0; i < num_encoded; i++) {
    Gate * g = PEEK (*e, i);
    if (!g) continue;
    if (g->op != INPUT) continue;
    fprintf (file, "c index %d input %d gate %d", i, g->input, g->idx);
    if (g->symbol) {
      fputs (" symbol ", file);
      const char * name = g->symbol->name;
      assert (name);
      fputs (name, file);
    }
    fputc ('\n', file);
  }
}

void print_dimacs_encoding (Encoding * e) {
  print_dimacs_encoding_to_file (e, stdout);
}

static Encoder * new_encoder (Circuit * circuit,
                              CNF * cnf, Encoding * encoding, int negative)
{
  Encoder * res;
  NEW (res);
  res->circuit = circuit;
  res->cnf = cnf;
  res->encoding = encoding;
  res->negative = negative;
  const int num_gates = COUNT (circuit->gates);
  ALLOC (res->map, num_gates);
  return res;
}

static void delete_encoder (Encoder * e) {
  const int num_gates = COUNT (e->circuit->gates);
  DEALLOC (e->map, num_gates);
  RELEASE (e->clause);
  RELEASE (e->marks);
  DELETE (e);
}

static int map_gate (Gate * g, Encoder * e) {
  int sign = SIGN (g);
  if (sign) g = NOT (g);
  int res = e->map[g->idx];
  assert (res);
  if (sign) res = -res;
  return res;
}

static int marked_lit (Encoder * e, int lit) {
  int idx = abs (lit);
  if (idx >= COUNT (e->marks)) return 0;
  int res = e->marks.start[idx];
  if (lit < 0) res = -res;
  return res;
}

static void mark_lit (Encoder * e, int lit) {
  int idx = abs (lit);
  while (idx >= COUNT (e->marks))
    PUSH (e->marks, 0);
  e->marks.start[idx] = lit;
}

static void unmark_clause_literals (Encoder * e) {
  int size = COUNT (e->marks);
  while (!EMPTY (e->clause)) {
    int lit = POP (e->clause);
    int idx = abs (lit);
    if (idx < size) e->marks.start[idx] = 0;
  }
}

static int clause_is_trivial (Encoder * e) {
  int * p = e->clause.start, * q = p;
  while (p < e->clause.top) {
    int lit = *p++, tmp = marked_lit (e, lit);
    if (tmp < 0) { LOG ("skipping trivial clause"); return 1; }
    if (tmp > 0) continue;
    mark_lit (e, lit);
    *q++ = lit;
  }
  if (q < p) {
    LOG ("removing %d duplicated literals", (int)(p - q));
    e->clause.top = q;
  }
  return 0;
}

static void encode_clause (Encoder * e) {
  if (!clause_is_trivial (e)) {
    const int size = COUNT (e->clause);
    Clause * c = new_clause (e->clause.start, size);
    c->negative = e->negative;
    add_clause_to_cnf (c, e->cnf);
  }
  unmark_clause_literals (e);
}

static void encode_unary (Encoder * e, int lit) {
  assert (EMPTY (e->clause));
  PUSH (e->clause, lit);
  encode_clause (e);
}

static void encode_binary (Encoder * e, int a, int b) {
  assert (EMPTY (e->clause));
  PUSH (e->clause, a);
  PUSH (e->clause, b);
  encode_clause (e);
}

static void encode_ternary (Encoder * e, int a, int b, int c) {
  assert (EMPTY (e->clause));
  PUSH (e->clause, a);
  PUSH (e->clause, b);
  PUSH (e->clause, c);
  encode_clause (e);
}

static int map_input (Gate * g, int input, Encoder * e) {
  return map_gate (get_gate_input (g, input), e);
}

static void encode_false (Gate * g, Encoder * e) {
  int lit = map_gate (g, e);
  LOG ("encoding FALSE gate %d with literal %d", g->idx, lit);
  encode_unary (e, -lit);
}

static void encode_and (Gate * g, Encoder * e) {
  const int n = get_gate_size (g);
  int lit = map_gate (g, e);
  LOG ("encoding %d-ary AND gate %d with literal %d", n, g->idx, lit);
  for (int i = 0; i < n; i++)
    encode_binary (e, -lit, map_input (g, i, e));
  assert (EMPTY (e->clause));
  PUSH (e->clause, lit);
  for (int i = 0; i < n; i++)
    PUSH (e->clause, -map_input (g, i, e));
  encode_clause (e);
}

static void encode_xor (Gate * g, Encoder * e) {
  int n = COUNT (g->inputs);
  assert (n > 1);
  int mapped = e->map[g->idx];
  LOG ("encoding %d-ary XOR gate %d with literal %d", n, g->idx, mapped);
  int idx = mapped - (n - 1);
  int a = map_input (g, 0, e);
  for (int i = 1; i < n; i++) {
    int lit = idx + i;
    int b = map_input (g, i, e);
    encode_ternary (e, -lit, a, b);
    encode_ternary (e, -lit, -a, -b);
    encode_ternary (e, lit, -a, b);
    encode_ternary (e, lit, a, -b);
    a = lit;
  }
  assert (a == mapped);
}

static void encode_or (Gate * g, Encoder * e) {
  const int n = get_gate_size (g);
  int lit = map_gate (g, e);
  LOG ("encoding %d-ary OR gate %d with literal %d", n, g->idx, lit);
  for (int i = 0; i < n; i++)
    encode_binary (e, lit, -map_input (g, i, e));
  assert (EMPTY (e->clause));
  PUSH (e->clause, -lit);
  for (int i = 0; i < n; i++)
    PUSH (e->clause, map_input (g, i, e));
  encode_clause (e);
}

static void encode_ite (Gate * g, Encoder * e) {
  assert (COUNT (g->inputs) == 3);
  int lit = map_gate (g, e);
  LOG ("encoding 3-ary ITE gate %d as literal %d", g->idx, lit);
  int cond = map_input (g, 0, e);
  int pos = map_input (g, 1, e);
  int neg = map_input (g, 2, e);
  encode_ternary (e, lit, -cond, -pos);
  encode_ternary (e, lit, cond, -neg);
  encode_ternary (e, -lit, -cond, pos);
  encode_ternary (e, -lit, cond, neg);
}

static void encode_xnor (Gate * g, Encoder * e) {
  int n = COUNT (g->inputs);
  assert (n > 1);
  int mapped = e->map[g->idx];
  LOG ("encoding %d-ary XNOR gate %d with literal %d", n, g->idx, mapped);
  int idx = mapped - (n - 1);
  int a = map_input (g, 0, e);
  for (int i = 1; i < n; i++) {
    int lit = idx + i;
    int b = map_input (g, i, e);
    encode_ternary (e, lit, a, b);
    encode_ternary (e, lit, -a, -b);
    encode_ternary (e, -lit, -a, b);
    encode_ternary (e, -lit, a, -b);
    a = lit;
  }
  assert (a == mapped);
}

static void encode_gate (Gate * g, Encoder *e) {
  switch (g->op) {
    case FALSE: encode_false (g, e); break;
    case AND: encode_and (g, e); break;
    case XOR: encode_xor (g, e); break;
    case OR: encode_or (g, e); break;
    case ITE: encode_ite (g, e); break;
    case XNOR: encode_xnor (g, e); break;
    case INPUT: /* UNREACHABLE */ break;
  }
}

static int encode_inputs_with_encoder (Encoder * e) {
  LOG ("starting to encode inputs");
  int * map = e->map, idx = 0;
  Gate ** p = e->circuit->inputs.start;
  while (p < e->circuit->inputs.top) {
    Gate * g = *p++;
    assert (g);
    assert (!SIGN (g));
    assert (g->op == INPUT);
    idx++;
    if (map) assert (!map[g->idx]), map[g->idx] = idx;
    encode_input (e->encoding, g, idx);
  }
  msg (2, "encoded %d inputs", idx);
  return idx;
}

static int encode_gates (Encoder * e, int idx) {
  LOG ("starting to encode gates");
  STACK (Gate *) stack;
  INIT (stack);
  PUSH (stack, STRIP (e->circuit->output));
  int encoded = 0, * map = e->map;
  while (!EMPTY (stack)) {
    Gate * g = TOP (stack);
    assert (!SIGN (g));
    if (g) {
      if (map[g->idx]) (void) POP (stack);
      else {
	assert (g->pos >0 || g->neg > 0);
	PUSH (stack, 0);
	int n = COUNT (g->inputs);
	LOG ("traversing %d-ary %s gate", n, gate_name (g));
	for (int i = n-1; i >= 0; i--) {
	  Gate * other = STRIP (g->inputs.start[i]);
	  if (!map[other->idx]) PUSH (stack, other);
	}
      }
    } else {
      (void) POP (stack);
      assert (!EMPTY (stack));
      g = POP (stack);
      assert (g);
      assert (!SIGN (g));
      if (map[g->idx]) continue;
      if (g->op == XOR || g->op == XNOR) {
	int n = COUNT (g->inputs);
	if (n > 2) {
	  LOG (
	    "reserving additional %d variables for %d-ary %s",
	    n-2, n, (g->op == XOR ? "XOR" : "XNOR"));
	  idx += n-2;
	}
      }
      map[g->idx] = ++idx;
      encode_gate (g, e);
      encoded++;
    }
  }
  RELEASE (stack);
  msg (2, "encoded %d non-input gates", encoded);
  return idx;
}

void encode_circuit (Circuit * circuit,
                     CNF * cnf, Encoding * encoding, int negative)
{
  assert (negative || !maximum_variable_index (cnf));
  cone_of_influence (circuit);
  Encoder * encoder = new_encoder (circuit, cnf, encoding, negative);
  int idx = encode_inputs_with_encoder (encoder);
  idx = encode_gates (encoder, idx);
  encode_unary (encoder, map_gate (circuit->output, encoder));
  delete_encoder (encoder);
  msg (2, "encoded %d gates in total", idx);
}

void get_encoded_inputs (Encoding * e, IntStack * inputs) {
  const int n = COUNT (*e);
  for (int i = 0; i < n; i++) {
    Gate * g = PEEK (*e, i);
    if (!g) continue;
    assert (!SIGN (g));
    if (g->op != INPUT) continue;
    PUSH (*inputs, i);
  }
  LOG ("found %ld encoded inputs", (long) COUNT (*inputs));
}
