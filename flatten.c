#include "headers.h"

static Gate REMOVE[1];

static int removable_gate_during_flattening (Gate * g) {
  return g->op == AND_OPERATOR || g->op == XOR_OPERATOR ||
         g->op == OR_OPERATOR ||  g->op == XNOR_OPERATOR;
}

static void mark_removed_during_flattening (Gate * g, Gate ** map) {
  assert (!SIGN (g));
  assert (!map [g->idx]);
  if (!g->pos && !g->neg) return;
  if (!removable_gate_during_flattening (g)) return;
  for (Gate ** p = g->inputs.start; p != g->inputs.top; p++) {
    Gate * h = *p;
    if (SIGN (h)) continue;
    assert (h->idx < g->idx);
    if (h->op != g->op) continue;
    if ((long)h->pos + (long)h->neg > 1) continue;
    assert (!map[h->idx]);
    map[h->idx] = REMOVE;
  }
}

static Gate * copy_input_gate_and_own_symbol (Gate * g, Circuit * c) {
  Gate * res = new_input_gate (c);
  Symbol * s = g->symbol;
  if (s) {
    assert (s->gate == g);
    res->symbol = s;
    s->gate = res;
    assert (s->name);
    LOG ("copy and owning input %d gate %d symbol '%s'",
      res->input, res->idx, s->name);
  }
  return res;
}

static Gate * flatten_gate (Gate *, Gate **, Gates *, Circuit *);

static void
flatten_tree (Gate * g, Gate ** map, Gates * gates, Circuit * c)
{
  assert (!SIGN (g));
  if (!g->pos && !g->neg) return;
  assert (removable_gate_during_flattening (g));
  for (Gate ** p = g->inputs.start; p != g->inputs.top; p++) {
    Gate * h = *p;
    if (!SIGN (h) && map[h->idx] == REMOVE) {
      assert (h->op == g->op);
      assert (h->pos + h->neg == 1);
      flatten_tree (h, map, gates, c);
    } else {
      Gate * res = flatten_gate (h, map, gates, c);
      PUSH (*gates, res);
    }
  }
}

static Gate *
flatten_gate (Gate * g, Gate ** map, Gates * gates, Circuit * c)
{
  const int sign = SIGN (g);
  if (sign) g = NOT (g);
  Gate * res = map[g->idx];
  if (res == REMOVE) { assert (!sign); return 0; }
  if (!res) {
    if (g->op == INPUT_OPERATOR)
      res = copy_input_gate_and_own_symbol (g, c);
    else if (!g->pos && !g->neg) return 0;
    else if (g->op == FALSE_OPERATOR) res = new_false_gate (c);
    else {
      if (g->op == ITE_OPERATOR) {
        res = new_ite_gate (c);
	Gate ** inputs = g->inputs.start;
	// IMPORTANT: do not change the order here!!!!
        connect_gates (flatten_gate (inputs[0], map, gates, c), res);
        connect_gates (flatten_gate (inputs[2], map, gates, c), res);
        connect_gates (flatten_gate (inputs[1], map, gates, c), res);
      } else {
	CLEAR (*gates);
	flatten_tree (g, map, gates, c);
	switch (g->op) {
	  default: assert (g->op == AND_OPERATOR); // fall through
	  case AND_OPERATOR:  res = new_and_gate (c); break;
	  case XOR_OPERATOR:  res = new_xor_gate (c); break;
	  case OR_OPERATOR:   res = new_or_gate (c); break;
	  case XNOR_OPERATOR: res = new_xnor_gate (c); break;
	}
	for (Gate ** p = gates->start; p != gates->top; p++)
	  connect_gates (*p, res);
      }
    }
    map[g->idx] = res;
  }
  if (sign) res = NOT (res);
  return res;
}

Circuit * flatten_circuit (Circuit *c) {
  cone_of_influence (c);
  LOG ("flatten circuit");
  check_circuit_connected (c);
  Circuit * res = new_circuit ();
  const int num_gates = COUNT (c->gates);
  Gate ** map;
  ALLOC (map, num_gates);
  Gates gates;
  INIT (gates);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    mark_removed_during_flattening (*p, map);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    (void) flatten_gate (*p, map, &gates, res);
  Gate * o = flatten_gate (c->output, map, &gates, res);
  assert (o);
  RELEASE (gates);
  connect_output (res, o);
  DEALLOC (map, num_gates);
  msg (1, "flattened circuit from %zd gates to %zd gates %.0f%%",
    COUNT (c->gates), COUNT (res->gates), 
    percent (COUNT (res->gates), COUNT (c->gates)));
  return res;
}
