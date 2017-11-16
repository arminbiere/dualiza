#include "headers.h"

#ifdef NLOG
#define POG(...) do { } while (0)
#define POGCLS(...) do { } while (0)
#else
#define POG(FMT,ARGS...) LOG ("%d " FMT, solver->level, ##ARGS)
#define POGCLS(C,FMT,ARGS...) LOGCLS (C, "%d " FMT, solver->level, ##ARGS)
#endif

typedef struct Var Var;
typedef struct Queue Queue;
typedef STACK (Var *) VarStack;

struct Var {
  char input, decision, seen;
  signed char val, phase;
  int level;
  Clause * reason;
  long stamp;
  Var * prev, * next;
  Clauses occs[2];
};

struct Queue {
  Var * first, * last, * search;
  long stamp;
};

struct Primal {
  Var * vars;
  int max_var, next, level;
  IntStack trail, clause, * sorted;
  VarStack seen;
  Queue inputs, gates;
  CNF * cnf;
};

static Var * var (Primal * solver, int lit) {
  assert (lit);
  int idx = abs (lit);
  assert (idx <= solver->max_var);
  return solver->vars + idx;
}

#ifndef NLOG

static int var2idx (Primal * solver, Var * v) {
  assert (solver->vars < v);
  assert (v <= solver->vars + solver->max_var);
  return (int)(long)(v - solver->vars);
}

#endif

static int val (Primal * solver, int lit) {
  Var * v = var (solver, lit);
  int res = v->val;
  if (lit < 0) res = -res;
  return res;
}

static Queue * queue (Primal * solver, Var * v) {
  return v->input ? &solver->inputs : &solver->gates;
}

static void update_queue (Primal * solver, Queue * q, Var * v) {
  assert (!v || q == queue (solver, v));
  q->search = v;
#ifndef NLOG
  const int idx = (v ? (long)(v - solver->vars) : 0l);
  const char * t = (q == &solver->inputs ? "input" : "gate");
  POG ("update %s search to variable %d", t, idx);
#endif
}

#ifndef NLOG

static const char * type (Var * v) {
  assert (v);
  return v->input ? "input" : "gate";
}

#endif

static void enqueue (Primal * solver, Var * v) {
  Queue * q = queue (solver, v);
  assert (!v->next);
  assert (!v->prev);
  v->stamp = ++q->stamp;
  POG ("enqueue %s variable %d stamp %ld",
    type (v), var2idx (solver, v), v->stamp);
  if (!q->first) q->first = v;
  if (q->last) q->last->next = v;
  v->prev = q->last;
  q->last = v;
  if (!v->val) update_queue (solver, q, v);
}

static void dequeue (Primal * solver, Var * v) {
  Queue * q = queue (solver, v);
  POG ("dequeue %s variable %d stamp %ld",
    type (v), var2idx (solver, v), v->stamp);
  if (v->prev) assert (v->prev->next == v), v->prev->next = v->next;
  else assert (q->first == v), q->first = v->next;
  if (v->next) assert (v->next->prev == v), v->next->prev = v->prev;
  else assert (q->last == v), q->last = v->prev;
  if (q->search == v) {
    Var * u = v->next;
    if (!u) u = v->prev;
    update_queue (solver, q, u);
  }
  v->next = v->prev = 0;
}

Primal * new_primal (CNF * cnf, IntStack * inputs) {
  assert (cnf);
  LOG ("new primal solver over %ld clauses and %ld inputs",
    (long) COUNT (cnf->clauses), (long) COUNT (*inputs));
  Primal * res;
  NEW (res);
  res->cnf = cnf;
  res->sorted = inputs;
  int max_var_in_cnf = maximum_variable_index (cnf);
  LOG ("maximum variable index %d in CNF", max_var_in_cnf);
  int max_var_in_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (0 < input);
    if (input > max_var_in_inputs) max_var_in_inputs = input;
  }
  LOG ("maximum variable index %d in inputs", max_var_in_inputs);
  res->max_var = MAX (max_var_in_cnf, max_var_in_inputs);
  LOG ("maximum variable index %d", res->max_var);
  ALLOC (res->vars, res->max_var + 1);
  for (int idx = 1; idx <= res->max_var; idx++)
    res->vars[idx].phase = -1;
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
#ifndef NLOG
  const int num_gates = res->max_var - num_inputs;
  LOG ("using %d inputs and %d gate variables", num_inputs, num_gates);
  LOG ("connecting variables");
#endif
  for (int idx = 1; idx <= res->max_var; idx++)
    enqueue (res, var (res, idx));
  assert (res->inputs.stamp == num_inputs);
  assert (res->gates.stamp == num_gates);
  return res;
}

void delete_primal (Primal * solver) {
  LOG ("deleting primal solver");
  RELEASE (solver->trail);
  RELEASE (solver->seen);
  RELEASE (solver->clause);
  for (int idx = 1; idx <= solver->max_var; idx++) {
    Var * v = solver->vars + idx;
    RELEASE (v->occs[0]);
    RELEASE (v->occs[1]);
  }
  DEALLOC (solver->vars, solver->max_var+1);
  DELETE (solver);
}

static void assign (Primal * solver, int lit, Clause * reason) {
  int idx = abs (lit);
  assert (0 < idx), assert (idx <= solver->max_var);
  Var * v = solver->vars + idx;
  if (!reason) POG ("assign %s %d decision", type (v), lit); 
  else POGCLS (reason, "assign %s %d reason", type (v), lit);
  assert (!v->val);
  if (lit < 0) v->val = v->phase = -1;
  else v->val = v->phase = 1;
  v->level = solver->level;
  v->reason = reason;
  PUSH (solver->trail, lit);
}

static Clauses * occs (Primal * solver, int lit) {
  return &var (solver, lit)->occs[lit < 0];
}


static void connect_literal (Primal * solver, Clause * c, int lit) {
  POGCLS (c, "connecting literal %d to", lit);
  Clauses * cs = occs (solver, lit);
  PUSH (*cs, c);
}

static void connect_clause (Primal * solver, Clause * c) {
  assert (c->size > 1);
  connect_literal (solver, c, c->literals[0]);
  connect_literal (solver, c, c->literals[1]);
}

static int connect_cnf (Primal * solver) {
  assert (!solver->level);
  Clauses * clauses = &solver->cnf->clauses;
  LOG ("connecting %ld clauses to Primal solver", (long) COUNT (*clauses));
  for (Clause ** p = clauses->start; p < clauses->top; p++) {
    Clause * c = *p;
    if (c->size == 0) {
      LOG ("found empty clause in CNF");
      return 0;
    } else if (c->size == 1) {
      int unit = c->literals[0];
      LOG ("found unit clause %d", unit);
      int tmp = val (solver, unit);
      if (tmp > 0) {
	LOG ("ignoring already satisfied unit %d", unit);
	continue;
      }
      if (tmp < 0) {
	LOG ("found already falsified unit %d", unit);
	return 0;
      }
      assign (solver, unit, c);
    } else {
      assert (c->size > 1);
      connect_clause (solver, c);
    }
  }
  return 1;
}

static Clause * bcp (Primal * solver) {
  Clause * res = 0;
  while (!res && solver->next < COUNT (solver->trail)) {
    int lit = solver->trail.start[solver->next++];
    assert (val (solver, lit) > 0);
    POG ("propagating %d", lit);
    propagated++;
    Clauses * o = occs (solver, -lit);
    Clause ** q = o->start, ** p = q;
    while (!res && p < o->top) {
      Clause * c = *q++ = *p++;
      POGCLS (c, "visiting while propagating %d", lit);
      assert (c->size > 1);
      if (c->literals[0] != -lit)
	SWAP (int, c->literals[0], c->literals[1]);
      assert (c->literals[0] == -lit);
      const int other = c->literals[1], other_val = val (solver, other);
      if (other_val > 0) continue;
      int i = 2, replacement_val = -1, replacement = 0;
      while (i < c->size) {
	replacement = c->literals[i];
	replacement_val = val (solver, replacement);
	if (replacement_val >= 0) break;
	i++;
      }
      if (replacement_val >= 0) {
	POGCLS (c, "disconnecting literal %d from", -lit);
	c->literals[0] = replacement;
	c->literals[i] = -lit;
	connect_literal (solver, c, replacement);
	q--;
      } else if (!other_val) {
	POGCLS (c, "forcing %d", other);
	assign (solver, other, c);
      } else {
	assert (other_val < 0);
	POGCLS (c, "conflicting");
	conflicts++;
	res = c;
      }
    }
    while (p < o->top) *q++ = *p++;
    o->top = q;
  }
  if (res) conflicts++;
  return res;
}

static int satisfied (Primal * solver) {
  return solver->next == solver->max_var;
}

static void inc_level (Primal * solver) {
  solver->level++;
  POG ("incremented decision level");
}

static void dec_level (Primal * solver) {
  assert (solver->level > 0);
  solver->level--;
  POG ("decremented decision level");
}

static Var * decide_input (Primal * solver) {
  Var * res = solver->inputs.search;
  while (res && res->val) 
    res = res->prev, searched++;
  update_queue (solver, &solver->inputs, res);
  return res;
}

static Var * decide_gate (Primal * solver) {
  Var * res = solver->gates.search;
  while (res && res->val)
    res = res->prev, searched++;
  update_queue (solver, &solver->gates, res);
  return res;
}

static void decide (Primal * solver) {
  Var * v = decide_input (solver);
  if (!v) v = decide_gate (solver);
  assert (v);
  int lit = v - solver->vars;
  if (v->phase < 0) lit = -lit;
  inc_level (solver);
  POG ("decide %s %d", type (v), lit);
  assign (solver, lit, 0);
  assert (!v->decision);
  v->decision = 1;
  decisions++;
}

static void unassign (Primal * solver, int lit) {
  Var * v = var (solver, lit);
  assert (solver->level == v->level);
  solver->level = v->level;		// TODO remove
  POG ("unassign %s %d", type (v), lit);
  assert (v->val);
  v->val = v->decision = 0;
  Queue * q = queue (solver, v);
  if (!q->search || q->search->stamp < v->stamp)
    update_queue (solver, q, v);
}

static int backtrack (Primal * solver) {
  POG ("backtrack");
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    const int decision = v->decision;
    if (decision == 1) {
      v->decision = 2;
      POG ("flip %s %d", type (v), lit);
      POG ("assign %s %d", type (v), -lit);
      int n = COUNT (solver->trail) - 1;
      POKE (solver->trail, n, -lit);
      solver->next = n;
      v->val = -v->val;
      return 1;
    }
    unassign (solver, lit);
    if (decision == 2) dec_level (solver);
    (void) POP (solver->trail);
  }
  solver->next = 0;
  return 0;
}

static int resolve_literal (Primal * solver, int lit) {
  Var * v = var (solver, lit);
  if (!v->level) return 0;
  if (v->seen) return 0;
  v->seen = 1;
  PUSH (solver->seen, v);
  POG ("resolved %s %d", type (v), lit);
  assert (val (solver, lit) < 0);
  if (v->level == solver->level) return 1;
  PUSH (solver->clause, lit);
  return 0;
}

static int resolve_clause (Primal * solver, Clause * c) {
  assert (c);
  POGCLS (c, "resolving");
  int unresolved = 0;
  for (int i = 0; i < c->size; i++)
    unresolved += resolve_literal (solver, c->literals[i]);
  return unresolved;
}

static int cmp_seen (const void * p, const void * q) {
  Var * u = * (Var **) p, * v = * (Var **) q;
  if (u->stamp < v->stamp) return -1;
  if (u->stamp > v->stamp) return 1;
  return 0;
}

static void sort_seen (Primal * solver) {
  const int n = COUNT (solver->seen);
  qsort (solver->seen.start, n, sizeof *solver->seen.start, cmp_seen);
}

static void bump_variable (Primal * solver, Var * v) {
  bumped++;
  POG ("bump %s variable %d", type (v), var2idx (solver, v));
  dequeue (solver, v);
  enqueue (solver, v);
}

static void bump_seen (Primal * solver) {
  sort_seen (solver);
  for (Var ** p = solver->seen.start; p < solver->seen.top; p++)
    bump_variable (solver, *p);
}

static void reset_seen (Primal * solver) {
  while (!EMPTY (solver->seen))
    POP (solver->seen)->seen = 0;
}

static int analyze (Primal * solver, Clause * conflict) {
  Clause * c = conflict;
  const int * p = solver->trail.top;
  int unresolved = 0, uip = 0;
  for (;;) {
    unresolved += resolve_clause (solver, c);
    POG ("unresolved literals %d", unresolved);
    assert (p > solver->trail.start);
    while (!var (solver, (uip = *--p))->seen)
      assert (p > solver->trail.start);
    POG ("analyze %d", uip);
    if (!--unresolved) break;
    c = var (solver, uip)->reason;
  }
  PUSH (solver->clause, uip);
  if (options.bump) bump_seen (solver);
  reset_seen (solver);
  CLEAR (solver->clause);
  return backtrack (solver);
}

int primal_sat (Primal * solver) {
  if (!connect_cnf (solver)) return 20;
  if (bcp (solver)) return 20;
  if (satisfied (solver)) return 10;
  int res = 0;
  while (!res) {
    Clause * conflict = bcp (solver);
    if (conflict) {
       if (!analyze (solver, conflict)) res = 20;
    } else if (satisfied (solver)) res = 10;
    else decide (solver);
  }
  return res;
}

void primal_count (Number res, Primal * solver) {
  if (!connect_cnf (solver)) return;
  if (bcp (solver)) return;
  if (satisfied (solver)) {
    POG ("single root level model");
    inc_number (res);
    return;
  }
  for (;;) {
    Clause * conflict = bcp (solver);
    if (conflict) {
      if (!analyze (solver, conflict)) return;
    } else if (satisfied (solver)) {
      POG ("new model");
      inc_number (res);
      if (!backtrack (solver)) return;
    } else decide (solver);
  }
}

static void print_model (Primal * solver, Name name) {
  const int n = COUNT (*solver->sorted);
  for (int i = 0; i < n; i++) {
    if (i) fputc (' ', stdout);
    const int lit = PEEK (*solver->sorted, i);
    const int tmp = val (solver, lit);
    assert (tmp);
    if (tmp < 0) fputc ('!', stdout);
    fputs (name.get (name.state, lit), stdout);
  }
  fputc ('\n', stdout);
}

void primal_enumerate (Primal * solver, Name name) {
  if (!connect_cnf (solver)) return;
  if (!bcp (solver)) return;
  if (satisfied (solver)) { print_model (solver, name); return; }
  for (;;) {
    Clause * conflict = bcp (solver);
    if (conflict) {
      if (!analyze (solver, conflict)) return;
    } else if (satisfied (solver)) {
      print_model (solver, name);
      if (!backtrack (solver)) return;
    } else decide (solver);
  }
}

int primal_deref (Primal * solver, int lit) {
  return val (solver, lit);
}
