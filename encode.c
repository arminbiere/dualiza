#include "headers.h"

typedef struct Encoder Encoder;

struct Encoder {
  Circuit * circuit;
  CNF * cnf;
  Encoding * encoding;
  int * map;
  int negative;
  STACK (int) clause, marks;
};

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

static void reset_clause (Encoder * e) {
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
  reset_clause (e);
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
  LOG ("encoding FALSE gate");
  encode_unary (e, map_gate (g, e));
}

static void encode_and (Gate * g, Encoder * e) {
  const int size = get_gate_size (g);
  LOG ("encoding AND gate of size %ld", size);
  int lit = map_gate (g, e);
  for (int i = 0; i < size; i++)
    encode_binary (e, -lit, map_input (g, i, e));
  assert (EMPTY (e->clause));
  PUSH (e->clause, lit);
  for (int i = 0; i < size; i++)
    PUSH (e->clause, -map_input (g, i, e));
  encode_clause (e);
}

static void encode_xor (Gate * g, Encoder * e) {
  assert (COUNT (g->inputs) == 2);
  LOG ("encoding XOR gate of size %ld", (long) COUNT (g->inputs));
  int lit = map_gate (g, e);
  int a = map_input (g, 0, e);
  int b = map_input (g, 1, e);
  encode_ternary (e, -lit, a, b);
  encode_ternary (e, -lit, -a, -b);
  encode_ternary (e, lit, -a, b);
  encode_ternary (e, lit, a, -b);
}

static void encode_or (Gate * g, Encoder * e) {
  const int size = get_gate_size (g);
  LOG ("encoding OR gate of size %ld", (long) COUNT (g->inputs));
  int lit = map_gate (g, e);
  for (int i = 0; i < size; i++)
    encode_binary (e, lit, -map_input (g, i, e));
  assert (EMPTY (e->clause));
  PUSH (e->clause, -lit);
  for (int i = 0; i < size; i++)
    PUSH (e->clause, map_input (g, i, e));
  encode_clause (e);
}

static void encode_ite (Gate * g, Encoder * e) {
  assert (COUNT (g->inputs) == 3);
  LOG ("encoding ITE gate of size %ld", (long) COUNT (g->inputs));
  int lit = map_gate (g, e);
  int cond = map_input (g, 0, e);
  int pos = map_input (g, 1, e);
  int neg = map_input (g, 2, e);
  encode_ternary (e, lit, -cond, -pos);
  encode_ternary (e, lit, cond, -neg);
  encode_ternary (e, -lit, -cond, pos);
  encode_ternary (e, -lit, cond, neg);
}

static void encode_xnor (Gate * g, Encoder * e) {
  assert (COUNT (g->inputs) == 2);
  LOG ("encoding XNOR gate of size %ld", (long) COUNT (g->inputs));
  int lit = map_gate (g, e);
  int a = map_input (g, 0, e);
  int b = map_input (g, 1, e);
  encode_ternary (e, lit, a, b);
  encode_ternary (e, lit, -a, -b);
  encode_ternary (e, -lit, -a, b);
  encode_ternary (e, -lit, a, -b);
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

static int encode_inputs (Encoder * e) {
  int * map = e->map, idx = 0;
  Gate ** p = e->circuit->inputs.start;
  while (p < e->circuit->inputs.top) {
    Gate * g = *p++;
    assert (g);
    assert (!SIGN (g));
    assert (g->op == INPUT);
    assert (!map[g->idx]);
    map[g->idx] = ++idx;
    encode_input (e->encoding, g, idx);
  }
  msg (2, "encoded %d inputs", idx);
  return idx;
}

static int encode_gates (Encoder * e, int idx) {
  int encoded = 0, * map = e->map;
  Gate ** p = e->circuit->gates.start;
  while (p < e->circuit->gates.top) {
    Gate * g = *p++;
    assert (g);
    assert (!SIGN (g));
    if (g->op == INPUT) { assert (map[g->idx]); continue; }
    assert (!map[g->idx]);
    if (!g->pos && !g->neg) continue;
    map[g->idx] = ++idx;
    encode_gate (g, e);
    encoded++;
  }
  msg (2, "encoded %d non-input gates", encoded);
  return idx;
}

void encode_circuit (Circuit * circuit,
                     CNF * cnf, Encoding * encoding, int negative)
{
  assert (negative || !maximum_variable_index (cnf));
  cone_of_influence (circuit);
  Encoder * encoder = new_encoder (circuit, cnf, encoding, negative);
  int idx = encode_inputs (encoder);
  idx = encode_gates (encoder, idx);
  encode_unary (encoder, map_gate (circuit->output, encoder));
  delete_encoder (encoder);
  msg (2, "encoded %d gates in total", idx);
}
