#include "headers.h"

static long
simulation_cache_index (Gate * g)
{
  int sign = SIGN (g);
  if (sign)
    g = NOT (g);
  long res = 2 * g->idx;
  if (sign)
    res++;
  return res;
}

static BDD *
cached_simulate (BDD ** cache, Gate * g)
{
  BDD *res = cache[simulation_cache_index (g)];
  if (res)
    res = copy_bdd (res);
  return res;
}

static void
cache_simulate (BDD ** cache, Gate * g, BDD * b)
{
  cache[simulation_cache_index (g)] = copy_bdd (b);
}

static BDD *simulate_circuit_recursive (Gate *, BDD **, Circuit *);

static BDD *
simulate_gates (Gate ** gates, long n,
		BDD ** cache, Circuit * c,
		BDD * (*op) (BDD *, BDD *), const char *name)
{
  assert (n > 0);
  if (n == 1)
    return simulate_circuit_recursive (gates[0], cache, c);
  LOG ("simulating %s over %ld gates", name, n);
  unsigned m = n / 2;
  BDD *l = simulate_gates (gates, m, cache, c, op, name);
  BDD *r = simulate_gates (gates + m, n - m, cache, c, op, name);
  BDD *res = op (l, r);
  delete_bdd (l);
  delete_bdd (r);
  return res;
}

static BDD *
simulate_circuit_recursive (Gate * g, BDD ** cache, Circuit * c)
{
  BDD *res = cached_simulate (cache, g);
  if (res)
    return res;
  if (SIGN (g))
    {
      LOG ("simulating NOT");
      BDD *tmp = simulate_circuit_recursive (NOT (g), cache, c);
      res = not_bdd (tmp);
      delete_bdd (tmp);
    }
  else
    {
      long n = COUNT (g->inputs);
      Gate **inputs = g->inputs.start;
      switch (g->op)
	{
	case FALSE_OPERATOR:
	  LOG ("simulating FALSE");
	  res = false_bdd ();
	  break;
	case INPUT_OPERATOR:
	  LOG ("simulating INPUT %d", g->input);
	  res = new_bdd (g->input + 1);
	  break;
	case AND_OPERATOR:
	  res = simulate_gates (inputs, n, cache, c, and_bdd, "AND");
	  break;
	case XOR_OPERATOR:
	  res = simulate_gates (inputs, n, cache, c, xor_bdd, "XOR");
	  break;
	case OR_OPERATOR:
	  res = simulate_gates (inputs, n, cache, c, or_bdd, "OR");
	  break;
	case ITE_OPERATOR:
	  LOG ("simulating ITE");
	  assert (n == 3);
	  {
	    BDD *cond = simulate_circuit_recursive (inputs[0], cache, c);
	    BDD *then = simulate_circuit_recursive (inputs[1], cache, c);
	    BDD *other = simulate_circuit_recursive (inputs[2], cache, c);
	    res = ite_bdd (cond, then, other);
	    delete_bdd (cond);
	    delete_bdd (then);
	    delete_bdd (other);
	  }
	  break;
	case XNOR_OPERATOR:
	  res = simulate_gates (inputs, n, cache, c, xnor_bdd, "XNOR");
	  break;
	}
    }
  cache_simulate (cache, g, res);
  return res;
}

BDD *
simulate_circuit (Circuit * c)
{
  check_circuit_connected (c);
  long count = 2 * COUNT (c->gates);
  BDD **cache;
  ALLOC (cache, count);
  BDD *res = simulate_circuit_recursive (c->output, cache, c);
  for (long i = 0; i < count; i++)
    {
      BDD *b = cache[i];
      if (b)
	delete_bdd (b);
    }
  DEALLOC (cache, count);
  return res;
}
