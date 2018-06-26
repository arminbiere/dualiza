#include "headers.h"

static Gate * new_gate (Circuit * c, Operator op) {
  assert (COUNT (c->gates) < INT_MAX);
  Gate * res;
  NEW (res);
  res->idx = COUNT (c->gates);
  res->circuit = c;
  res->op = op;
  PUSH (c->gates, res);
  LOG ("new %s gate %d", gate_name (res), res->idx);
  return res;
}

int get_gate_size (Gate * g) {
  g = STRIP (g);
  return COUNT (g->inputs);
}

Gate * get_gate_input (Gate * g, int input) {
  g = STRIP (g);
  assert (0 <= input);
  assert (input < COUNT (g->inputs));
  return g->inputs.start[input];
}

Gate * new_false_gate (Circuit * c) {
  if (c->zero) return c->zero;
  return c->zero = new_gate (c, FALSE_OPERATOR);
}

Gate * new_input_gate (Circuit * c) {
  Gate * res = new_gate (c, INPUT_OPERATOR);
  res->input = COUNT (c->inputs);
  PUSH (c->inputs, res);
  return res;
}

Gate * new_and_gate (Circuit * c) { return new_gate (c, AND_OPERATOR); }
Gate * new_xor_gate (Circuit * c) { return new_gate (c, XOR_OPERATOR); }
Gate * new_or_gate (Circuit * c) { return new_gate (c, OR_OPERATOR); }
Gate * new_ite_gate (Circuit * c) { return new_gate (c, ITE_OPERATOR); }
Gate * new_xnor_gate (Circuit * c) { return new_gate (c, XNOR_OPERATOR); }

Circuit * new_circuit () {
  Circuit * res;
  NEW (res);
  return res;
}

static void delete_gate (Gate * g) {
  assert (!SIGN (g));
  RELEASE (g->inputs);
  RELEASE (g->outputs);
  DELETE (g);
}

void delete_circuit (Circuit * c) {
  msg (2, "deleting circuit with %ld gates", COUNT (c->gates));
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    delete_gate (*p);
  RELEASE (c->inputs);
  RELEASE (c->gates);
  DELETE (c);
}

#ifndef NLOG
static const char * ordinal_suffix (int n) {
  if (n == 1) return "st";
  if (n == 2) return "nd";
  if (n == 3) return "rd";
  return "th";
}
#endif

void connect_gates (Gate * input, Gate * output) {
  Gate * stripped_input = STRIP (input);
  Gate * stripped_output = STRIP (output);
  assert (input);
  assert (output);
  assert (stripped_input->circuit == stripped_output->circuit);
  assert (stripped_output->op != FALSE_OPERATOR);
  assert (stripped_output->op != INPUT_OPERATOR);
  PUSH (stripped_input->outputs, output);
  PUSH (stripped_output->inputs, input);
#ifndef NLOG
  int count = COUNT (stripped_output->inputs);
  LOG (
    "%s gate %d connected%s as %d%s input to %s gate %d",
    gate_name (stripped_input), stripped_input->idx,
    SIGN (input) ? " negated" : "",
    count, ordinal_suffix (count),
    gate_name (stripped_output), stripped_output->idx);
#endif
}

void connect_output (Circuit * c, Gate * output) {
  assert (c);
  assert (!c->output);
  assert (((Gate*)STRIP (output))->circuit == c);
  c->output = output;
}

const char * gate_name (Gate * g) {
  if (SIGN (g)) return "NOT";
  switch (g->op) {
    case FALSE_OPERATOR: return "FALSE";
    case INPUT_OPERATOR: return "INPUT";
    case AND_OPERATOR: return "AND";
    case XOR_OPERATOR: return "XOR";
    case OR_OPERATOR: return "OR";
    case ITE_OPERATOR: return "ITE";
    case XNOR_OPERATOR: return "XNOR";
    default: return "UNKNOWN";
  }
}

void check_circuit_connected (Circuit * c) {
#ifndef NDEBUG
  assert (c);
  assert (c->output);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++) {
    Gate * g = *p;
    assert (!SIGN (g));
    const int num_inputs = COUNT (g->inputs);
    if (g->op == FALSE_OPERATOR) assert (num_inputs == 0);
    else if (g->op == INPUT_OPERATOR) assert (num_inputs == 0);
    else if (g->op == ITE_OPERATOR) assert (num_inputs == 3);
    else assert (num_inputs > 1);
  }
#endif
}

static int sort_gate (Gate * g, int * map, int idx) {
  if (SIGN (g)) g = NOT (g);
  if (map[g->idx] >= 0) return idx;
  for (Gate ** p = g->inputs.start; p != g->inputs.top; p++)
    idx = sort_gate (*p, map, idx);
  return map[g->idx] = ++idx;
}

void sort_circuit (Circuit * c) {
  LOG ("sorting circuit");
  const int n = COUNT (c->gates);
  int * map;
  ALLOC (map, n);
  for (int i = 0; i < n; i++) map[i] = -1;
  int idx = 0;
  for (Gate ** p = c->inputs.start; p != c->inputs.top; p++)
    idx = sort_gate (*p, map, idx);
  assert (idx == (int) COUNT (c->inputs));
  for (Gate ** p = c->gates.top; p != c->gates.start; p--)
    idx = sort_gate (p[-1], map, idx);
  assert (idx == n);
  int cmp (const void * p, const void * q) {
    Gate * g = * (Gate **) p, * h = * (Gate **) q;
    int i = map[g->idx], j = map[h->idx];
    return i - j;
  }
  qsort (c->gates.start, n, sizeof (Gate*), cmp);
  DEALLOC (map, n);
  for (idx = 0; idx < n; idx++) {
    Gate * g = PEEK (c->gates, idx);
    if (g->idx == idx) continue;
    LOG ("changing %s gate %d to %d", gate_name (g), g->idx, idx);
    g->idx = idx;
#ifndef NDEBUG
    for (Gate ** p = g->inputs.start; p != g->inputs.top; p++) {
      Gate * h = STRIP (*p);
      assert (h->idx < g->idx);
    }
#endif
  }
  LOG ("sorted circuit");
}
