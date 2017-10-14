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

Gate * new_false_gate (Circuit * c) {
  if (c->zero) return c->zero;
  return c->zero = new_gate (c, FALSE);
}

Gate * new_input_gate (Circuit * c, int input) {
  Gate * res = new_gate (c, INPUT);
  res->input = input;
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
  assert (STRIP (output)->circuit == c);
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
      case XOR:
      case XNOR:
        assert (num_inputs == 2);
	break;
      case ITE:
        assert (num_inputs == 3);
	break;
      case FALSE:
      case INPUT:
      case AND:
      case OR:
        break;
    }
  }
#endif
}

static void print_gate_to_file (Gate * g, Operator outer, FILE * file) {
  const int sign = SIGN (g);
  if (sign) g = NOT (g);
  if (g->op == FALSE) fprintf (file, "%c", sign ? '1' : '0');
  else {
    if (sign) fprintf (file, "!");
    if (g->op == INPUT) fprintf (file, "x%d", g->input);
    else {
      int parenthesis;
      if (sign) parenthesis = 1;
      else if (g->op < ITE) parenthesis = (g->op > outer);
      else parenthesis = (g->op >= outer);
      if (parenthesis) fprintf (file, "(");
      if (g->op == ITE) {
	Gate ** inputs = g->inputs.start;
	assert (COUNT (g->inputs) == 3);
	print_gate_to_file (inputs[0], ITE, file);
	fprintf (file, " ? ");
	print_gate_to_file (inputs[1], ITE, file);
	fprintf (file, " : ");
	print_gate_to_file (inputs[2], ITE, file);
      } else {
	char op = '=';
	if (g->op == AND) op = '&';
	else if (g->op == OR) op = '|';
	else if (g->op == XOR) op = '^';
	else assert (g->op == XNOR);
	for (Gate ** p = g->inputs.start; p < g->inputs.top; p++) {
	  if (p != g->inputs.start) fprintf (file, " %c ", op);
	  print_gate_to_file (*p, g->op, file);
	}
      }
      if (parenthesis) fprintf (file, ")");
    }
  }
}

void println_gate (Gate * g) {
  print_gate_to_file (g, -1, stdout);
  fputc ('\n', stdout);
}

void print_circuit_to_file (Circuit * c, FILE * file) {
  check_circuit_connected (c);
  print_gate_to_file (c->output, -1, file);
}

void println_circuit (Circuit * c) {
  print_circuit_to_file (c, stdout);
  fputc ('\n', stdout);
}

