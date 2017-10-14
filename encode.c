#include "headers.h"

void encode_circuit (Circuit * c, CNF * f, Encoding * e, int neg) {
  assert (neg || !maximum_variable_index (f));
  cone_of_influence (c);
  int num_inputs = COUNT (c->inputs);
  int num_gates = COUNT (c->gates);
  int * map;
  ALLOC (map, num_gates);
  int idx = 0;
  for (int i = 0; i < num_inputs; i++) {
    Gate * g = c->inputs.start[i];
    assert (g->op == INPUT);
    assert (g->idx < num_gates);
    assert (!map[g->idx]);
    map[g->idx] = ++idx;
    encode_gate (e, g, idx);
  }
  msg (2, "encoded %d inputs", idx);
  DEALLOC (map, num_gates);
}
