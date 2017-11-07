#include "headers.h"

#ifdef NLOG
#define DOG(...) do { } while (0)
#define DOGCLS(...) do { } while (0)
#else
#define DOG(FMT,ARGS...) LOG ("%d " FMT, ##ARGS)
#define DOGCLS(C,FMT,ARGS...) LOGCLS (C, "%d " FMT, ##ARGS)
#endif

typedef struct Var Var;

struct Var {
  char input;
  signed char val, phase;
  int level;
  Var * prev, * next;
  Clauses occs[2];
};

struct DPLL {
  int max_var;
  int next;
  int level;
  IntStack trail;
  Var * vars;
  CNF * cnf;
  struct {
    long conflicts;
    long decisions;
    long propagations;
  } stats;
};

static Var * var (DPLL * dpll, int lit) {
  assert (lit);
  int idx = abs (lit);
  assert (idx <= dpll->max_var);
  return dpll->vars + idx;
}

static int val (DPLL * dpll, int lit) {
  Var * v = var (dpll, lit);
  int res = v->val;
  if (lit < 0) res = -res;
  return res;
}

DPLL * new_dpll (CNF * cnf, IntStack * inputs) {
  assert (cnf);
  LOG ("new DPLL over %ld clauses and %ld inputs",
    (long) COUNT (cnf->clauses), (long) COUNT (*inputs));
  DPLL * res;
  NEW (res);
  int max_var_in_cnf = maximum_variable_index (cnf);
  LOG ("maximum variable index %d in CNF", max_var_in_cnf);
  int max_var_in_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (0 < input);
    if (input > max_var_in_inputs) max_var_in_inputs = input;
  }
  LOG ("maximum input variable index %d", max_var_in_inputs);
  res->max_var = MAX (max_var_in_inputs, max_var_in_inputs);
  LOG ("maximum variable index %d", res->max_var);
  ALLOC (res->vars, res->max_var + 1);
  int num_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (input < res->max_var);
    Var * v = var (res, input);
    if (v->input) continue;
    LOG ("input variable %d", input);
    num_inputs++;
    v->input = 1;
  }
  LOG ("using %d inputs and %d non-inputs variables",
    num_inputs, res->max_var - num_inputs);
  for (int idx = 1; idx <= res->max_var; idx++)
    res->vars[idx].phase = -1;
  return res;
}

void delete_dpll (DPLL * dpll) {
  LOG ("deleting DPLL solver");
  RELEASE (dpll->trail);
  for (int idx = 1; idx <= dpll->max_var; idx++) {
    Var * v = dpll->vars + idx;
    RELEASE (v->occs[0]);
    RELEASE (v->occs[1]);
  }
  DEALLOC (dpll->vars, dpll->max_var+1);
  DELETE (dpll);
}

static void assign (DPLL * dpll, int lit) {
  DOG ("assign %d", lit);
  assert (!val (dpll, lit));
  int idx = abs (lit);
  Var * v = dpll->vars + idx;
  if (lit < 0) v->val = v->phase = -1;
  else v->val = v->phase = 1;
  v->level = dpll->level;
  PUSH (dpll->trail, lit);
}

static Clauses * occs (DPLL * dpll, int lit) {
  return &var (dpll, lit)->occs[lit < 0];
}


static void connect_literal (DPLL * dpll, Clause * c, int lit) {
  DOGCLS (c, "connecting literal %d to", lit);
  Clauses * cs = occs (dpll, lit);
  PUSH (*cs, c);
}

static void connect_clause (DPLL * dpll, Clause * c) {
  assert (c->size > 1);
  connect_literal (dpll, c, c->literals[0]);
  connect_literal (dpll, c, c->literals[1]);
}

static int connect_cnf (DPLL * dpll) {
  assert (!dpll->level);
  Clauses * clauses = &dpll->cnf->clauses;
  LOG ("connecting %ld clauses to DPLL solver", (long) COUNT (*clauses));
  for (Clause ** p = clauses->start; p < clauses->top; p++) {
    Clause * c = *p;
    if (c->size == 0) {
      LOG ("found empty clause in CNF");
      return 0;
    } else if (c->size == 1) {
      int unit = c->literals[0];
      LOG ("found unit clause %d", unit);
      int tmp = val (dpll, unit);
      if (tmp > 0) {
	LOG ("ignoring already satisfied unit %d", unit);
	continue;
      }
      if (tmp < 0) {
	LOG ("found already falsified unit %d", unit);
	return 0;
      }
      assign (dpll, unit);
    } else {
      assert (c->size > 1);
      connect_clause (dpll, c);
    }
  }
  return 1;
}

static int bcp (DPLL * dpll) {
  int res = 1;
  while (res && dpll->next < COUNT (dpll->trail)) {
    int lit = dpll->trail.start[dpll->next++];
    assert (val (dpll, lit) > 0);
    LOG ("propagating %d", lit);
    dpll->stats.propagations++;
    Clauses * o = occs (dpll, -lit);
    Clause ** q = o->start, ** p = q;
    while (res && p < o->top) {
      Clause * c = *q++ = *p++;
      DOGCLS (c, "visiting while propagating %d", lit);
      assert (c->size > 1);
      if (c->literals[0] != -lit)
	SWAP (int, c->literals[0], c->literals[1]);
      assert (c->literals[0] == -lit);
      const int other = c->literals[1], other_val = val (dpll, other);
      if (other_val > 0) continue;
      int i = 2, replacement_val = -1, replacement = 0;
      while (i < c->size) {
	replacement = c->literals[i];
	replacement_val = val (dpll, replacement);
	if (replacement_val >= 0) break;
	i++;
      }
      if (replacement_val >= 0) {
	DOGCLS (c, "disconnecting literal %d from", -lit);
	c->literals[0] = replacement;
	c->literals[i] = -lit;
	connect_literal (dpll, c, replacement);
	q--;
      } else if (!other_val) {
	DOGCLS (c, "forcing %d", other);
	assign (dpll, other);
      } else {
	assert (other_val < 0);
	DOGCLS (c, "conflicting");
	res = 0;
      }
    }
    while (p < o->top) *q++ = *p++;
    o->top = q;
  }
  return res;
}

static void connect_inputs (DPLL * dpll) {
  LOG ("connecting inputs");
}

int dpll_sat (DPLL * dpll) {
  if (!connect_cnf (dpll)) return 20;
  if (!bcp (dpll)) return 20;
  connect_inputs (dpll);
  return 0;
}

int dpll_deref (DPLL * dpll, int lit) {
  return val (dpll, lit);
}

void dpll_stats (DPLL * dpll) {
  if (verbosity < 1) return;
  msg (1,
    "%ld decisions, %ld propagations, %ld decisions",
    dpll->stats.decisions,
    dpll->stats.propagations,
    dpll->stats.conflicts);
}
