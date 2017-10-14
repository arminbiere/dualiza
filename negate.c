#include "headers.h"

static Gate * negate_gate (Gate * g, Gate ** map, Circuit * c) {
  const int sign = SIGN (g);
  if (sign) g = NOT (g);
  Gate * res = map[g->idx];
  if (!res) {
    Gate ** inputs = g->inputs.start;
    switch (g->op) {
      case FALSE:
        res = NOT (new_false_gate (c));
	break;
      case INPUT:
        res = NOT (new_input_gate (c));
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
        res = new_ite_gate (c);
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
  Circuit * res = new_circuit ();
  const int num_gates = COUNT (c->gates);
  Gate ** map;
  ALLOC (map, num_gates);
  Gate * o = negate_gate (c->output, map, res);
  connect_output (res, o);
  DEALLOC (map, num_gates);
  return res;
}
