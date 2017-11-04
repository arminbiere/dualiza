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
    delete_bdd (tmp);
  } else {
    long num_inputs = COUNT (g->inputs);
    Gate ** inputs = g->inputs.start;
    switch (g->op) {
      case FALSE:
	res = false_bdd ();
        break;
      case INPUT:
        res = new_bdd (g->input);
        break;
      case AND:
        res = true_bdd ();
	for (long i = 0; i < num_inputs; i++) {
	  BDD * tmp = simulate_circuit_recursive (inputs[i], cache, c);
	  BDD * and = and_bdd (res, tmp);
	  delete_bdd (tmp);
	  delete_bdd (res);
	  res = and;
	}
        break;
      case XOR:
	assert (num_inputs == 2);
	{
	  BDD * l = simulate_circuit_recursive (inputs[0], cache, c);
	  BDD * r = simulate_circuit_recursive (inputs[1], cache, c);
	  res = xor_bdd (l, r);
	  delete_bdd (l);
	  delete_bdd (r);
	}
        break;
      case OR:
        res = false_bdd ();
	for (long i = 0; i < num_inputs; i++) {
	  BDD * tmp = simulate_circuit_recursive (inputs[i], cache, c);
	  BDD * or = or_bdd (res, tmp);
	  delete_bdd (tmp);
	  delete_bdd (res);
	  res = or;
	}
        break;
      case ITE:
	assert (num_inputs == 3);
	{
	  BDD * cond = simulate_circuit_recursive (inputs[0], cache, c);
	  BDD * then = simulate_circuit_recursive (inputs[1], cache, c);
	  BDD * other = simulate_circuit_recursive (inputs[2], cache, c);
	  res = ite_bdd (cond, then, other);
	  delete_bdd (cond);
	  delete_bdd (then);
	  delete_bdd (other);
	}
        break;
      case XNOR:
	assert (num_inputs == 2);
	{
	  BDD * l = simulate_circuit_recursive (inputs[0], cache, c);
	  BDD * r = simulate_circuit_recursive (inputs[1], cache, c);
	  res = xnor_bdd (l, r);
	  delete_bdd (l);
	  delete_bdd (r);
	}
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
