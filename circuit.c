#include "headers.h"

static Gate * new_gate (Circuit * c, Operator op) {
  assert (COUNT (c->gates) < INT_MAX);
  Gate * res;
  NEW (res);
  res->idx = COUNT (c->gates);
  res->circuit = c;
  res->op = op;
  PUSH (c->gates, res);
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
  return c->zero = new_gate (c, FALSE);
}

Gate * new_input_gate (Circuit * c) {
  Gate * res = new_gate (c, INPUT);
  res->input = COUNT (c->inputs);
  PUSH (c->inputs, res);
  return res;
}

Gate * new_and_gate (Circuit * c) { return new_gate (c, AND); }
Gate * new_xor_gate (Circuit * c) { return new_gate (c, XOR); }
Gate * new_or_gate (Circuit * c) { return new_gate (c, OR); }
Gate * new_ite_gate (Circuit * c) { return new_gate (c, ITE); }
Gate * new_xnor_gate (Circuit * c) { return new_gate (c, XNOR); }

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

void connect_gates (Gate * input, Gate * output) {
  Gate * stripped_input = STRIP (input);
  Gate * stripped_output = STRIP (output);
  assert (input);
  assert (output);
  assert (stripped_input->circuit = stripped_output->circuit);
  assert (stripped_output->op != FALSE);
  assert (stripped_output->op != INPUT);
  PUSH (stripped_input->outputs, output);
  PUSH (stripped_output->inputs, input);
}

void connect_output (Circuit * c, Gate * output) {
  assert (c);
  assert (!c->output);
  assert (((Gate*)STRIP (output))->circuit == c);
  c->output = output;
}

void check_circuit_connected (Circuit * c) {
#ifndef NDEBUG
  assert (c);
  assert (c->output);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++) {
    Gate * g = *p;
    assert (!SIGN (g));
    const int num_inputs = COUNT (g->inputs);
    switch (g->op) {
      case ITE:
        assert (num_inputs == 3);
	break;
      case FALSE:
      case INPUT:
      case AND:
      case XOR:
      case OR:
      case XNOR:
        break;
    }
  }
#endif
}
