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
  msg (1, "new circuit with %d inputs", num_inputs);
  return res;
}

static void delete_gate (Gate * g) {
  assert (!SIGN (g));
  RELEASE (g->inputs);
  RELEASE (g->outputs);
  DELETE (g);
}

void delete_circuit (Circuit * c) {
  msg (1, "deleting circuit with %ld gates", COUNT (c->gates));
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

Gate * new_or_gate (Circuit * c) {
  Gate * res = new_gate (c);
  assert (res->idx >= c->num_inputs);
  res->op = OR;
  return res;
}

Gate * new_xor_gate (Circuit * c) {
  Gate * res = new_gate (c);
  assert (res->idx >= c->num_inputs);
  res->op = XOR;
  return res;
}

Gate * new_xnor_gate (Circuit * c) {
  Gate * res = new_gate (c);
  assert (res->idx >= c->num_inputs);
  res->op = XNOR;
  return res;
}

Gate * new_ite_gate (Circuit * c) {
  Gate * res = new_gate (c);
  assert (res->idx >= c->num_inputs);
  res->op = ITE;
  return res;
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
    case OR:
    case AND:
      for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
	coi (c, *p, sign);
      break;
    case ITE:
      for (Gate ** p = g->inputs.start; p < g->inputs.top; p++) {
	coi (c, *p, sign);
	if (p == g->inputs.start) coi (c, *p, !sign);
      }
      break;
    case XOR:
    case XNOR:
      for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
	coi (c, *p, sign), coi (c, *p, !sign);
      break;
    case FALSE:
    case INPUT:
      break;
  }
}

void connect_output (Circuit * c, Gate * output) {
  assert (c);
  assert (!c->output);
  assert (STRIP (output)->circuit == c);
  c->output = output;
}

static void check_circuit_connected (Circuit * c) {
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

void cone_of_influence (Circuit * c) {
  check_circuit_connected (c);
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
  msg (1, "coi: %d pos, %d neg, %d both, %d disconnected",
    pos, neg, both, disconnected);
}

static void print_gate_to_file (Gate * g, Operator outer, FILE * file) {
  const int sign = SIGN (g);
  if (sign) g = NOT (g);
  if (g->op == FALSE) fprintf (file, "%c", sign ? '1' : '0');
  else {
    if (sign) fprintf (file, "!");
    if (g->op == INPUT) fprintf (file, "x%d", g->idx);
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

static Gate * negate_gate (Gate * g, Gate ** map, Circuit * c) {
  const int sign = SIGN (g);
  if (sign) g = NOT (g);
  Gate * res = map[g->idx];
  if (!res) {
    Gate ** inputs = g->inputs.start;
    switch (g->op) {
      case FALSE:
        res = NOT (get_false_gate (c));
	break;
      case INPUT:
        res = NOT (get_input_gate (c, g->idx));
	break;
      case AND:
        res = new_or_gate (c);
	goto CONNECT;
      case XOR:
        res = new_xnor_gate (c);
	goto CONNECT;
      case OR:
        res = new_and_gate (c);
	goto CONNECT;
      case XNOR:
        res = new_xor_gate (c);
      CONNECT:
	for (Gate ** p = inputs; p < g->inputs.top; p++)
	  connect_gates (negate_gate (*p, map, c), res);
	break;
      case ITE:
        connect_gates (negate_gate (inputs[0], map, c), res);
        connect_gates (negate_gate (inputs[2], map, c), res); /* yes 2 !! */
        connect_gates (negate_gate (inputs[1], map, c), res); /* yes 1 !! */
        break;
    }
    map[g->idx] = res;
  }
  if (sign) res = NOT (res);
  return res;
}

Circuit * negate_circuit (Circuit * c) {
  check_circuit_connected (c);
  Circuit * res = new_circuit (c->num_inputs);
  const int num_gates = COUNT (c->gates);
  Gate ** map;
  ALLOC (map, num_gates);
  Gate * o = negate_gate (c->output, map, res);
  connect_output (res, o);
  DEALLOC (map, num_gates);
  cone_of_influence (res);
  return res;
}
