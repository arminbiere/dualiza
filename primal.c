#include "headers.h"

#ifdef NLOG
#define POG(...) do { } while (0)
#define POGCLS(...) do { } while (0)
#else
#define POG(FMT,ARGS...) LOG ("%d " FMT, ##ARGS)
#define POGCLS(C,FMT,ARGS...) LOGCLS (C, "%d " FMT, primal->level, ##ARGS)
#endif

typedef struct Var Var;

struct Var {
  char input;
  signed char val, phase;
  int level;
  Var * prev, * next;
  Clauses occs[2];
};

struct Primal {
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

static Var * var (Primal * primal, int lit) {
  assert (lit);
  int idx = abs (lit);
  assert (idx <= primal->max_var);
  return primal->vars + idx;
}

static int val (Primal * primal, int lit) {
  Var * v = var (primal, lit);
  int res = v->val;
  if (lit < 0) res = -res;
  return res;
}

Primal * new_primal (CNF * cnf, IntStack * inputs) {
  assert (cnf);
  LOG ("new primal solver over %ld clauses and %ld inputs",
    (long) COUNT (cnf->clauses), (long) COUNT (*inputs));
  Primal * res;
  NEW (res);
  res->cnf = cnf;
  int max_var_in_cnf = maximum_variable_index (cnf);
  LOG ("maximum variable index %d in CNF", max_var_in_cnf);
  int max_var_in_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (0 < input);
    if (input > max_var_in_inputs) max_var_in_inputs = input;
  }
  LOG ("maximum variable index %d in inputs", max_var_in_inputs);
  res->max_var = MAX (max_var_in_inputs, max_var_in_inputs);
  LOG ("maximum variable index %d", res->max_var);
  ALLOC (res->vars, res->max_var + 1);
  int num_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (input <= res->max_var);
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

void delete_primal (Primal * primal) {
  LOG ("deleting primal solver");
  RELEASE (primal->trail);
  for (int idx = 1; idx <= primal->max_var; idx++) {
    Var * v = primal->vars + idx;
    RELEASE (v->occs[0]);
    RELEASE (v->occs[1]);
  }
  DEALLOC (primal->vars, primal->max_var+1);
  DELETE (primal);
}

static void assign (Primal * primal, int lit) {
  POG ("assign %d", lit);
  assert (!val (primal, lit));
  int idx = abs (lit);
  Var * v = primal->vars + idx;
  if (lit < 0) v->val = v->phase = -1;
  else v->val = v->phase = 1;
  v->level = primal->level;
  PUSH (primal->trail, lit);
}

static Clauses * occs (Primal * primal, int lit) {
  return &var (primal, lit)->occs[lit < 0];
}


static void connect_literal (Primal * primal, Clause * c, int lit) {
  POGCLS (c, "connecting literal %d to", lit);
  Clauses * cs = occs (primal, lit);
  PUSH (*cs, c);
}

static void connect_clause (Primal * primal, Clause * c) {
  assert (c->size > 1);
  connect_literal (primal, c, c->literals[0]);
  connect_literal (primal, c, c->literals[1]);
}

static int connect_cnf (Primal * primal) {
  assert (!primal->level);
  Clauses * clauses = &primal->cnf->clauses;
  LOG ("connecting %ld clauses to Primal solver", (long) COUNT (*clauses));
  for (Clause ** p = clauses->start; p < clauses->top; p++) {
    Clause * c = *p;
    if (c->size == 0) {
      LOG ("found empty clause in CNF");
      return 0;
    } else if (c->size == 1) {
      int unit = c->literals[0];
      LOG ("found unit clause %d", unit);
      int tmp = val (primal, unit);
      if (tmp > 0) {
	LOG ("ignoring already satisfied unit %d", unit);
	continue;
      }
      if (tmp < 0) {
	LOG ("found already falsified unit %d", unit);
	return 0;
      }
      assign (primal, unit);
    } else {
      assert (c->size > 1);
      connect_clause (primal, c);
    }
  }
  return 1;
}

static int bcp (Primal * primal) {
  int res = 1;
  while (res && primal->next < COUNT (primal->trail)) {
    int lit = primal->trail.start[primal->next++];
    assert (val (primal, lit) > 0);
    LOG ("propagating %d", lit);
    primal->stats.propagations++;
    Clauses * o = occs (primal, -lit);
    Clause ** q = o->start, ** p = q;
    while (res && p < o->top) {
      Clause * c = *q++ = *p++;
      POGCLS (c, "visiting while propagating %d", lit);
      assert (c->size > 1);
      if (c->literals[0] != -lit)
	SWAP (int, c->literals[0], c->literals[1]);
      assert (c->literals[0] == -lit);
      const int other = c->literals[1], other_val = val (primal, other);
      if (other_val > 0) continue;
      int i = 2, replacement_val = -1, replacement = 0;
      while (i < c->size) {
	replacement = c->literals[i];
	replacement_val = val (primal, replacement);
	if (replacement_val >= 0) break;
	i++;
      }
      if (replacement_val >= 0) {
	POGCLS (c, "disconnecting literal %d from", -lit);
	c->literals[0] = replacement;
	c->literals[i] = -lit;
	connect_literal (primal, c, replacement);
	q--;
      } else if (!other_val) {
	POGCLS (c, "forcing %d", other);
	assign (primal, other);
      } else {
	assert (other_val < 0);
	POGCLS (c, "conflicting");
	res = 0;
      }
    }
    while (p < o->top) *q++ = *p++;
    o->top = q;
  }
  return res;
}

static void connect_inputs (Primal * primal) {
  LOG ("connecting inputs");
}

static int satisfied (Primal * primal) {
  return primal->next == primal->max_var;
}

int primal_sat (Primal * primal) {
  if (!connect_cnf (primal)) return 20;
  if (!bcp (primal)) return 20;
  if (satisfied (primal)) return 10;
  connect_inputs (primal);
  return 0;
}

int primal_deref (Primal * primal, int lit) {
  return val (primal, lit);
}

void primal_stats (Primal * primal) {
  if (verbosity < 1) return;
  msg (1,
    "%ld decisions, %ld propagations, %ld decisions",
    primal->stats.decisions,
    primal->stats.propagations,
    primal->stats.conflicts);
}
