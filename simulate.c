#include "headers.h"

static long simulation_cache_index (Gate * g) {
  int sign = SIGN (g);
  if (sign) g = NOT (g);
  long res = 2*g->idx;
  if (sign) res++;
  return res;
}

static BDD * cached_simulate (BDD ** cache, Gate * g) {
  BDD * res = cache[simulation_cache_index (g)];
  if (res) res = copy_bdd (res);
  return res;
}

static void cache_simulate (BDD ** cache, Gate * g, BDD * b) {
  cache[simulation_cache_index (g)] = copy_bdd (b);
}

static BDD * simulate_circuit_recursive (Gate * g, BDD ** cache, Circuit * c) {
  BDD * res = cached_simulate (cache, g);
  if (res) return res;
  if (SIGN (g)) {
    BDD * tmp = simulate_circuit_recursive (NOT (g), cache, c);
    res = not_bdd (tmp);
    delete_bdd (res);
  } else {
    switch (g->op) {
      case FALSE:
	res = false_bdd ();
        break;
      case INPUT:
        break;
      case AND:
        break;
      case XOR:
        break;
      case OR:
        break;
      case ITE:
        break;
      case XNOR:
        break;
    }
  }
  cache_simulate (cache, g, res);
  return res;
}

BDD * simulate_circuit (Circuit * c) {
  check_circuit_connected (c);
  long count = 2*COUNT (c->inputs);
  BDD ** cache;
  ALLOC (cache, count);
  BDD * res = simulate_circuit_recursive (c->output, cache, c);
  for (long i = 0; i < count; i++) {
    BDD * b = cache[i];
    if (b) delete_bdd (b);
  }
  DEALLOC (cache, count);
  return res;
}
