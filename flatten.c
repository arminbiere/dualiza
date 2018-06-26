#include "headers.h"

static Gate REMOVE[1];

static int removable_gate_during_flattening (Gate * g) {
  return g->op == AND_OPERATOR || g->op == XOR_OPERATOR ||
         g->op == OR_OPERATOR ||  g->op == XNOR_OPERATOR;
}

static void mark_disconnected (Gate * g, Gate ** map) {
  assert (!SIGN (g));
  assert (!map [g->idx]);
  if (g->op != INPUT_OPERATOR && !g->pos && !g->neg) {
    LOG ("marking gate %d as removed", g->idx);
    map[g->idx] = REMOVE;
  } else LOG ("keeping gate %d", g->idx);
  g->pos = g->neg = 0;
}

static void count_occurrences (Gate * g, Gate ** map) {
  assert (!SIGN (g));
  if (g->op == INPUT_OPERATOR) return;
  if (map [g->idx] == REMOVE) return;
  for (Gate ** p = g->inputs.start; p != g->inputs.top; p++) {
    Gate * h = *p;
    int sign = SIGN (h);
    if (sign) h = NOT (h);
    assert (h->idx < g->idx);
    if (sign) h->neg++; else h->pos++;
  }
}

static void mark_removed (Gate * g, Gate ** map) {
  assert (!SIGN (g));
  if (g->op == INPUT_OPERATOR) return;
  if (map [g->idx] == REMOVE) return;
  assert (!map [g->idx]);
  if (!removable_gate_during_flattening (g)) return;
  for (Gate ** p = g->inputs.start; p != g->inputs.top; p++) {
    Gate * h = *p;
    if (SIGN (h)) continue;
    assert (h->idx < g->idx);
    if (h->op != g->op) continue;
    if (h->neg) continue;
    if (h->pos != 1) continue;
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

static Gate * flattened_gate (Gate * g, Gate ** map) {
  const int sign = SIGN (g);
  if (sign) g = NOT (g);
  Gate * res = map[g->idx];
  if (!res) return 0;
  if (res == REMOVE) return 0;
  if (sign) res = NOT (res);
  return res;
}

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
      Gate * res = flattened_gate (h, map);
      assert (res);
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
  if (res == REMOVE) return 0;
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
    mark_disconnected (*p, map);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    count_occurrences (*p, map);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    mark_removed (*p, map);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++)
    (void) flatten_gate (*p, map, &gates, res);
  Gate * o = flattened_gate (c->output, map);
  assert (o);
  RELEASE (gates);
  connect_output (res, o);
  DEALLOC (map, num_gates);
  msg (1, "flattened circuit from %zd gates to %zd gates %.0f%%",
    COUNT (c->gates), COUNT (res->gates), 
    percent (COUNT (res->gates), COUNT (c->gates)));
  return res;
}
