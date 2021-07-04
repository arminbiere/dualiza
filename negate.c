#include "headers.h"

static Gate *
copy_input_gate_and_share_symbol (Gate * g, Circuit * c)
{
  Gate *res = new_input_gate (c);
  Symbol *s = g->symbol;
  if (s)
    {
      res->symbol = s;
      assert (s->name);
      LOG ("sharing input %d gate %d symbol '%s'",
	   res->input, res->idx, s->name);
    }
  return res;
}

static Gate *
negate_gate (Gate * g, Gate ** map, Circuit * c)
{
  const int sign = SIGN (g);
  if (sign)
    g = NOT (g);
  Gate *res = map[g->idx];
  if (!res)
    {
      if (g->op == FALSE_OPERATOR)
	res = NOT (new_false_gate (c));
      else if (g->op == INPUT_OPERATOR)
	res = NOT (copy_input_gate_and_share_symbol (g, c));
      else
	{
	  if (g->op == ITE_OPERATOR)
	    {
	      res = new_ite_gate (c);
	      Gate **inputs = g->inputs.start;
	      // IMPORTANT: do not change the order here!!!!
	      connect_gates (negate_gate (inputs[0], map, c), res);
	      connect_gates (negate_gate (inputs[2], map, c), res);
	      connect_gates (negate_gate (inputs[1], map, c), res);
	    }
	  else
	    {
	      switch (g->op)
		{
		default:
		  assert (g->op == AND_OPERATOR);	// fall through
		case AND_OPERATOR:
		  res = new_or_gate (c);
		  break;
		case XOR_OPERATOR:
		  res = new_xnor_gate (c);
		  break;
		case OR_OPERATOR:
		  res = new_and_gate (c);
		  break;
		case XNOR_OPERATOR:
		  res = new_xor_gate (c);
		  break;
		}
	      for (Gate ** p = g->inputs.start; p != g->inputs.top; p++)
		connect_gates (negate_gate (*p, map, c), res);
	    }
	}
      map[g->idx] = res;
    }
  if (sign)
    res = NOT (res);
  return res;
}

Circuit *
negate_circuit (Circuit * c)
{
  LOG ("negate circuit");
  check_circuit_connected (c);
  Circuit *res = new_circuit ();
  const int num_gates = COUNT (c->gates);
  Gate **map;
  ALLOC (map, num_gates);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    (void) negate_gate (*p, map, res);
  Gate *o = negate_gate (c->output, map, res);
  connect_output (res, o);
  DEALLOC (map, num_gates);
  return res;
}
