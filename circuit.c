#include "headers.h"

static Gate * new_gate (Circuit * c) {
  assert (COUNT (c->gates) < INT_MAX);
  Gate * res;
  NEW (res);
  res->idx = COUNT (c->gates);
  res->circuit = c;
  PUSH (c->gates, res);
  return res;
}

static Gate * new_false_gate (Circuit * c) {
  assert (EMPTY (c->gates));
  Gate * res = new_gate (c);
  res->op = FALSE;
  assert (!res->idx);
  return res;
}

static Gate * new_input_gate (Circuit * c) {
  Gate * res = new_gate (c);
  res->op = INPUT;
  assert (0 < res->idx);
  assert (res->idx <= c->num_inputs);
  return res;
}

Circuit * new_circuit (int num_inputs) {
  Circuit * res;
  NEW (res);
  new_false_gate (res);
  res->num_inputs = num_inputs;
  for (int i = 0; i < num_inputs; i++)
    new_input_gate (res);
  msg ("new circuit with %d inputs", num_inputs);
  return res;
}

static void delete_gate (Gate * g) {
  DELETE (g);
}

void delete_circuit (Circuit * c) {
  msg ("deleting circuit with %ld gates", COUNT (c->gates));
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    delete_gate (*p);
  RELEASE (c->gates);
  DELETE (c);
}

Gate * new_and_gate (Circuit * c) {
  Gate * res = new_gate (c);
  assert (res->idx >= c->num_inputs);
  res->op = AND;
  return res;
}

void connect_gates (Gate * input, Gate * output) {
  input = STRIP (input);
  output = STRIP (output);
  assert (input);
  assert (output);
  assert (input->circuit = output->circuit);
  assert (output->op != FALSE);
  assert (output->op != INPUT);
  PUSH (input->outputs, output);
  PUSH (output->inputs, input);
}

Gate * get_false_gate (Circuit * c) {
  assert (c);
  assert (!EMPTY (c->gates));
  return c->gates.start[0];
}

Gate * get_input_gate (Circuit * c, int idx) {
  assert (c);
  assert (0 < idx);
  assert (idx <= c->num_inputs);
  return c->gates.start[idx];
}

static void coi (Circuit * c, Gate * g, int sign) {
  if (SIGN (g)) sign = !sign, g = NOT (g);
  assert (c);
  assert (g);
  const int mark = (1<<sign);
  if (g->mark & mark) return;
  g->mark |= mark;
  if (sign) g->neg++; else g->pos++;
  switch (g->op) {
    case AND:
      for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
	coi (c, *p, sign);
      break;
    case ITE:
      die ("coi for ITE operator not implemented yet");
      break;
    case XOR:
      die ("coi for XOR operator not implemented yet");
      break;
    case INPUT:
      break;
    case FALSE:
    default:
      assert (g->op == FALSE);
      break;
  }
}

void connect_output (Circuit * c, Gate * output) {
  assert (c);
  assert (!c->output);
  assert (STRIP (output)->circuit == c);
  c->output = output;
}

void cone_of_influence (Circuit * c) {
  assert (c);
  assert (c->output);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++) {
    Gate * g = *p;
    g->pos = g->neg = g->mark = 0;
  }
  coi (c, c->output, 0);
  int pos = 0, neg = 0, both = 0, disconnected = 0;
  for (Gate ** p = c->gates.start; p < c->gates.top; p++) {
    Gate * g = *p;
    assert (!SIGN (g));
    if (g->pos && g->neg) both++;
    else if (g->pos) pos++;
    else if (g->neg) neg++;
    else disconnected++;
    g->mark = 0;
  }
  msg ("coi: %d pos, %d neg, %d both, %d disconnected",
    pos, neg, both, disconnected);
}

