#include "headers.h"

void tseitin_encoding (Circuit * circuit, CNF * cnf, int negative) {
  assert (negative || !maximum_variable_index (cnf));
  cone_of_influence (circuit);
  int num_inputs = COUNT (circuit->inputs);
  int num_gates = COUNT (circuit->gates);
  int * map;
  ALLOC (map, num_gates);
  DEALLOC (map, num_gates);
  int idx = 0;
  for (int i = 0; i < num_inputs; i++) {
    Gate * g = circuit->inputs.start[i];
    assert (g->op == INPUT);
    assert (g->idx < num_gates);
    assert (!map[g->idx]);
    map[g->idx] = ++idx;
  }
  msg (2, "encoded %d inputs", idx);
}
