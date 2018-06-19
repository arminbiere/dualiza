#include "headers.h"

typedef struct Encoder Encoder;

struct Encoder {
  CNF * cnf;
  Circuit * circuit;
  STACK (int) clause, marks;
};

Gate * decode_literal (Circuit * circuit, int lit) {
  assert (lit), assert (lit != INT_MIN);
  int idx = abs (lit) - 1;
  if (idx >= COUNT (circuit->inputs)) return 0;
  Gate * res  = PEEK (circuit->inputs, idx);
  assert (res);
  assert (!SIGN (res));
  assert (res->input == idx);
  if (lit < 0) res = NOT (res);
  return res;
}

int encode_input (Circuit * circuit, Gate * g) {
  int sign = SIGN (g);
  if (sign) g = STRIP (g);
  assert (g->op == INPUT_OPERATOR);
  assert (g->circuit == circuit);
  assert (PEEK (circuit->inputs, g->input) == g);
  assert (g->input < INT_MAX);
  int res = g->input + 1;
#ifndef NLOG
  if (g->symbol) {
    assert (g->symbol->name);
    LOG ("encoding input %d symbol %s as %d",
      g->input, g->symbol->name, res);
  } else
    LOG ("encoding input %d as %d", g->input, res);
#endif
  if (sign) res = -res;
  return res;
}

void print_dimacs_encoding_to_file (Circuit * c, FILE * file) {
  for (Gate ** p = c->inputs.start; p < c->inputs.top; p++) {
    Gate * g = *p;
    int lit = encode_input (c, g);
    fprintf (file, "c index %d input %d gate %d", lit, g->input, g->idx);
    if (g->symbol) {
      fputs (" symbol ", file);
      const char * name = g->symbol->name;
      assert (name);
      fputs (name, file);
    }
    fputc ('\n', file);
  }
}

void print_dimacs_encoding (Circuit * circuit) {
  print_dimacs_encoding_to_file (circuit, stdout);
}

static Encoder * new_encoder (Circuit * circuit, CNF * cnf) {
  Encoder * res;
  NEW (res);
  res->circuit = circuit;
  res->cnf = cnf;
  return res;
}

static void delete_encoder (Encoder * e) {
  RELEASE (e->clause);
  RELEASE (e->marks);
  DELETE (e);
}

static int map_gate (Gate * g, Encoder * e) {
  int sign = SIGN (g);
  if (sign) g = NOT (g);
  int res = g->code;
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
    c->dual = e->cnf->dual;
    LOGCLS (c, "encoded new");
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
  if (options.polarity && g->pos && !g->neg) return;
  PUSH (e->clause, lit);
  for (int i = 0; i < n; i++)
    PUSH (e->clause, -map_input (g, i, e));
  encode_clause (e);
}

static void encode_xor (Gate * g, Encoder * e) {
  int n = COUNT (g->inputs);
  assert (n > 1);
  int mapped = g->code;
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
  int mapped = g->code;
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
    case FALSE_OPERATOR: encode_false (g, e); break;
    case AND_OPERATOR: encode_and (g, e); break;
    case XOR_OPERATOR: encode_xor (g, e); break;
    case OR_OPERATOR: encode_or (g, e); break;
    case ITE_OPERATOR: encode_ite (g, e); break;
    case XNOR_OPERATOR: encode_xnor (g, e); break;
    case INPUT_OPERATOR: /* UNREACHABLE */ break;
  }
}

static int encode_inputs (Encoder * e) {
  Circuit * c = e->circuit;
  Gate ** p = c->inputs.start;
  int res = 0;
  while (p < c->inputs.top) {
    Gate * g = *p++;
    assert (g);
    assert (!SIGN (g));
    assert (g->op == INPUT_OPERATOR);
    assert (!g->code);
    const int idx = encode_input (c, g);
    if (idx > res) res = idx;
    g->code = idx;
  }
  assert (res == COUNT (c->inputs));
  msg (2, "encoded %d inputs", res);
  return res;
}

static int encode_gates (Encoder * encoder, Gate * g, int idx) {
  g = STRIP (g);
  if (g->code) return idx;
  for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
    idx = encode_gates (encoder, *p, idx);
  if (g->op == XOR_OPERATOR || g->op == XNOR_OPERATOR) {
    const int n = COUNT (g->inputs);
    if (n > 2) {
      LOG ("reserving additional %d variables for %d-ary %s",
	n-2, n, (g->op == XOR_OPERATOR ? "XOR" : "XNOR"));
      idx += n-2;
    }
  }
  g->code = ++idx;
  encode_gate (g, encoder);
  return idx;
}

static int encode_root (Encoder * encoder, Gate * g, int idx) {
  const int sign = SIGN (g), bit = 1<<sign;
  Gate * h = sign ? NOT (g) : g;
  if (h->root & bit) return idx;
  h->root |= bit;
  LOG ("%sroot %zd-ary %s gate",
    sign ? "negated " : "", COUNT (h->inputs), gate_name (h));
  if (!h->code) idx = encode_gates (encoder, h, idx);
  encode_unary (encoder, map_gate (g, encoder));
  return idx;
}

static int encode_roots (Encoder * encoder, Gate * g, int idx) {
  const int sign = SIGN (g);
  if (!sign && g->op == AND_OPERATOR) {
    if (g->root & 1) return idx;
    for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
      idx = encode_roots (encoder, *p, idx);
    g->root |= 1;
  } else if (!sign && g->op == OR_OPERATOR) {
    if (g->root & 1) return idx;
    assert (EMPTY (encoder->clause));
    for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
      idx = encode_gates (encoder, *p, idx);
    for (Gate ** p = g->inputs.start; p < g->inputs.top; p++) {
      const int lit = map_gate (*p, encoder);
      PUSH (encoder->clause, lit);
    }
    encode_clause (encoder);
    g->root |= 1;
  } else idx = encode_root (encoder, g, idx);
  return idx;
}

static void reset_code_root_fields_of_circuit (Circuit * c) {
  LOG ("resetting code and root fields of circuit");
  for (Gate ** p = c->gates.start; p < c->gates.top; p++) {
    Gate * g = *p;
    g->code = g->root = 0;
  }
}

void encode_circuit (Circuit * circuit, CNF * cnf) {
  cone_of_influence (circuit);
  Encoder * encoder = new_encoder (circuit, cnf);
  reset_code_root_fields_of_circuit (circuit);
  int inputs = encode_inputs (encoder);
  LOG ("starting to encode roots");
  int idx = encode_roots (encoder, circuit->output, inputs);
  delete_encoder (encoder);
  msg (2, "encoded %d inputs and gates in total", idx);
}

void encode_circuits (Circuit * c, Circuit * d, CNF * f, CNF * g) {
  assert (COUNT (c->inputs) == COUNT (d->inputs));
  LOG ("starting to encode first circuit");
  cone_of_influence (c);
  Encoder * encoder = new_encoder (c, f);
  reset_code_root_fields_of_circuit (c);
  int inputs1 = encode_inputs (encoder);
  LOG ("starting to encode roots of first circuit");
  int first = encode_roots (encoder, c->output, inputs1);
  assert (first >= inputs1);
  delete_encoder (encoder);
  msg (2, "encoded %d gates of first circuit", first - inputs1);
  LOG ("starting to encode second circuit");
  encoder = new_encoder (d, g);
  reset_code_root_fields_of_circuit (d);
  int inputs2 = encode_inputs (encoder);
  assert (inputs1 == inputs2), (void) inputs2;
  LOG ("starting to encode roots of second circuit");
  int second = encode_roots (encoder, d->output, first);
  assert (second >= first);
  delete_encoder (encoder);
  msg (2, "encoded %d gates of second circuit", second - first);
  msg (2, "encoded %d inputs and gates in total", second);
}

void get_encoded_inputs (Circuit * c, IntStack * s) {
  for (Gate ** p = c->inputs.start; p < c->inputs.top; p++)
    PUSH (*s, encode_input (c, *p));
  LOG ("found %ld encoded inputs", (long) COUNT (*s));
}
