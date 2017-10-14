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
    int lit = *p, tmp = marked_lit (e, lit);
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
    add_clause_to_cnf (c, e->cnf);
  }
  reset_clause (e);
}

static int map_input (Gate * g, int input, Encoder * e) {
  return map_gate (get_gate_input (g, input), e);
}

static void encode_false (Gate * g, Encoder * e) {
  LOG ("encoding FALSE gate");
  int output_lit = map_gate (g, e);
  add_clause_to_cnf (new_unary_clause (output_lit), e->cnf);
}

static void encode_and (Gate * g, Encoder * e) {
  int size = get_gate_size (g);
  LOG ("encoding AND gate of size %ld", size);
  int output_lit = map_gate (g, e);
  for (int i = 0; i < size; i++) {
    int input_lit = map_input (g, i, e);
    PUSH (e->clause, -output_lit);
    PUSH (e->clause, input_lit);
    encode_clause (e);
  }
}

static void encode_xor (Gate * g, Encoder * e) {
  LOG ("encoding XOR gate of size %ld", (long) COUNT (g->inputs));
}

static void encode_or (Gate * g, Encoder * e) {
  LOG ("encoding OR gate of size %ld", (long) COUNT (g->inputs));
}

static void encode_ite (Gate * g, Encoder * e) {
  LOG ("encoding ITE gate of size %ld", (long) COUNT (g->inputs));
}

static void encode_xnor (Gate * g, Encoder * e) {
  LOG ("encoding XNOR gate of size %ld", (long) COUNT (g->inputs));
}

static void encode_gate (Gate * g, Encoder *e) {
  switch (g->op) {
    case FALSE: encode_false (g, e); break;
    case AND: encode_and (g, e); break;
    case XOR: encode_xor (g, e); break;
    case OR: encode_or (g, e); break;
    case ITE: encode_ite (g, e); break;
    case XNOR: encode_xnor (g, e); break;
    case INPUT: break;
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
  delete_encoder (encoder);
  msg (2, "encoded %d gates in total", idx);
}
