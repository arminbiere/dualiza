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
typedef struct Frame Frame;
typedef struct Limit Limit;

typedef STACK (Var *) VarStack;
typedef STACK (Frame) FrameStack;

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

struct Frame {
  int decision;
  char flipped, seen;
  int prev_flipped;
};

struct Limit {
  struct {
    long learned, interval, increment;
    int fixed;
  } reduce;
  struct {
    long conflicts;
    double slow, fast;
  } restart;
  struct { long conflicts; } rephase;
};

struct Primal {
  Var * vars;
  char iterating;
  int max_var, next, level, flipped, fixed, phase;
  IntStack trail, clause, levels, * sorted;
  FrameStack frames;
  VarStack seen;
  Queue inputs, gates;
  Limit limit;
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
  if (!options.inputs) return &solver->inputs;
  return v->input ? &solver->inputs : &solver->gates;
}

static void update_queue (Primal * solver, Queue * q, Var * v) {
  assert (!v || q == queue (solver, v));
  q->search = v;
#ifndef NLOG
  const int idx = (v ? (long)(v - solver->vars) : 0l);
  const char * t = (q == &solver->inputs ? "input" : "gate");
  POG ("%s update %d", t, idx);
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
  POG ("%s enqueue variable %d stamp %ld",
    type (v), var2idx (solver, v), v->stamp);
  if (!q->first) q->first = v;
  if (q->last) q->last->next = v;
  v->prev = q->last;
  q->last = v;
  if (!v->val) update_queue (solver, q, v);
}

static void dequeue (Primal * solver, Var * v) {
  Queue * q = queue (solver, v);
  POG ("%s dequeue variable %d stamp %ld",
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

static Frame new_frame (Primal * solver, int decision) {
  Frame res = { decision, 0, 0, solver->flipped };
  return res;
}

static Frame * frame (Primal * solver, int lit) {
  Var * v = var (solver, lit);
  assert (v->level < COUNT (solver->frames));
  return solver->frames.start + v->level;
}

static void init_reduce_limit (Primal * solver) {
  solver->limit.reduce.learned = MAX (options.reduceinit, 0);
  solver->limit.reduce.interval = MAX (options.reduceinit, 0);
  solver->limit.reduce.increment = MAX (options.reduceinc, 1);
  LOG ("initial reduce interval %ld", solver->limit.reduce.interval);
  LOG ("initial reduce increment %ld", solver->limit.reduce.increment);
  LOG ("initial reduce learned limit %ld",
    solver->limit.reduce.learned);
}

static void init_restart_limit (Primal * solver) {
  solver->limit.restart.conflicts = MAX (options.restartint, 1);
  LOG ("initial restart conflict limit %ld",
    solver->limit.restart.conflicts);
}

static void init_rephase_limit (Primal * solver) {
  solver->limit.rephase.conflicts = MAX (options.rephaseint, 1);
  LOG ("initial rephase conflict limit %ld",
    solver->limit.rephase.conflicts);
}

static void init_limits (Primal * solver) {
  init_reduce_limit (solver);
  init_restart_limit (solver);
  init_rephase_limit (solver);
}

Primal * new_primal (CNF * cnf, IntStack * inputs) {
  assert (cnf);
  Primal * solver;
  NEW (solver);
  LOG ("new primal solver over %ld clauses and %ld inputs",
    (long) COUNT (cnf->clauses), (long) COUNT (*inputs));
  solver->cnf = cnf;
  solver->sorted = inputs;
  int max_var_in_cnf = maximum_variable_index (cnf);
  LOG ("maximum variable index %d in CNF", max_var_in_cnf);
  int max_var_in_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (0 < input);
    if (input > max_var_in_inputs) max_var_in_inputs = input;
  }
  LOG ("maximum variable index %d in inputs", max_var_in_inputs);
  solver->max_var = MAX (max_var_in_cnf, max_var_in_inputs);
  LOG ("maximum variable index %d", solver->max_var);
  ALLOC (solver->vars, solver->max_var + 1);
  solver->phase = options.phaseinit ? 1 : -1;
  LOG ("default initial phase %d", solver->phase);
  for (int idx = 1; idx <= solver->max_var; idx++)
    solver->vars[idx].phase = solver->phase;
  int num_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (input <= solver->max_var);
    Var * v = var (solver, input);
    if (v->input) continue;
    LOG ("input variable %d", input);
    num_inputs++;
    v->input = 1;
  }
#if !defined(NLOG) || !defined(NDEBUG)
  const int num_gates = solver->max_var - num_inputs;
  LOG ("using %d inputs and %d gate variables", num_inputs, num_gates);
  LOG ("connecting variables");
#endif
  for (int idx = 1; idx <= solver->max_var; idx++)
    enqueue (solver, var (solver, idx));
#ifndef NDEBUG
  if (options.inputs) {
    assert (solver->inputs.stamp == num_inputs);
    assert (solver->gates.stamp == num_gates);
  }
#endif
  PUSH (solver->frames, new_frame (solver, 0));
  assert (COUNT (solver->frames) == solver->level + 1);
  init_limits (solver);
  return solver;
}

void delete_primal (Primal * solver) {
  LOG ("deleting primal solver");
  RELEASE (solver->frames);
  RELEASE (solver->trail);
  RELEASE (solver->seen);
  RELEASE (solver->clause);
  RELEASE (solver->levels);
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
  if (!reason) POG ("%s assign %d decision", type (v), lit); 
  else POGCLS (reason, "%s assign %d reason", type (v), lit);
  assert (!v->val);
  if (lit < 0) v->val = v->phase = -1;
  else v->val = v->phase = 1;
  v->level = solver->level;
  if (v->level) {
    v->reason = reason;
    if (reason) mark_clause_active (reason, solver->cnf);
  } else {
    solver->fixed++;
    v->reason = 0;
  }
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

static int remaining_variables (Primal * solver) {
  return solver->max_var - solver->fixed;
}

static void header (Primal * solver) {
  msg (1, "");
  msg (1,
    "  "
    "    time"
    "  memory"
    " conflicts"
    "  learned"
    "   clauses"
    " variables");
  msg (1, "");
}

static void report (Primal * solver, int verbosity, const char type) {
  if (options.verbosity < verbosity) return;
  if (!(stats.reports++ % 20)) header (solver);
  msg (1,
    "%c"
    " %8.2f"
    " %7.1f"
    " %9ld"
    " %8ld"
    " %9ld"
    " %8ld",
    type,
    process_time (),
    current_resident_set_size ()/(double)(1<<20),
    stats.conflicts,
    solver->cnf->redundant,
    solver->cnf->irredundant,
    remaining_variables (solver));
}

static Clause * bcp (Primal * solver) {
  Clause * res = 0;
  while (!res && solver->next < COUNT (solver->trail)) {
    int lit = solver->trail.start[solver->next++];
    assert (val (solver, lit) > 0);
    POG ("propagating %d", lit);
    stats.propagated++;
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
	stats.conflicts++;
	POGCLS (c, "conflict %ld", stats.conflicts);
	res = c;
      }
    }
    while (p < o->top) *q++ = *p++;
    o->top = q;
  }
  if (solver->iterating) {
    solver->iterating = 0;
    report (solver, 1, 'i');
  }
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
  Frame f = POP (solver->frames);
  POG ("popped %s frame", f.flipped ? "flipped" : "decision");
  if (f.prev_flipped != solver->flipped) {
    assert (f.prev_flipped < solver->flipped);
    assert (solver->flipped == solver->level + 1);
    solver->flipped = f.prev_flipped;
    POG ("restored flipped level %d", solver->flipped);
  }
  assert (COUNT (solver->frames) == solver->level + 1);
}

static Var * next_input (Primal * solver) {
  Var * res = solver->inputs.search;
  while (res && res->val) 
    res = res->prev, stats.searched++;
  update_queue (solver, &solver->inputs, res);
  return res;
}

static Var * next_gate (Primal * solver) {
  Var * res = solver->gates.search;
  while (res && res->val)
    res = res->prev, stats.searched++;
  update_queue (solver, &solver->gates, res);
  return res;
}

static Var * next_decision (Primal * solver) {
  Var * v = next_input (solver);
  if (!v) v = next_gate (solver);
  assert (v);
  POG ("next %s decision %d stamped %ld",
    type (v), var2idx (solver, v), v->stamp);
  return v;
}

static void decide (Primal * solver) {
  Var * v = next_decision (solver);
  int lit = v - solver->vars;
  if (v->phase < 0) lit = -lit;
  inc_level (solver);
  POG ("%s decide %d", type (v), lit);
  assign (solver, lit, 0);
  assert (!v->decision);
  v->decision = 1;
  stats.decisions++;
  PUSH (solver->frames, new_frame (solver, lit));
  assert (COUNT (solver->frames) == solver->level + 1);
}

static void unassign (Primal * solver, int lit) {
  Var * v = var (solver, lit);
  assert (solver->level == v->level);
  POG ("%s unassign %d", type (v), lit);
  assert (v->val);
  v->val = v->decision = 0;
  if (v->reason) mark_clause_inactive (v->reason, solver->cnf);
  Queue * q = queue (solver, v);
  if (!q->search || q->search->stamp < v->stamp)
    update_queue (solver, q, v);
}

static void block (Primal * solver, int lit) {
  assert (solver->level > 0);
  stats.blocked++;
  POG ("adding blocking clause for decision %d", lit);
  assert (val (solver, lit) > 0);
  Var * v = var (solver, lit);
  assert (v->decision == 1);
  for (int level = solver->level; level > 0; level--) {
    Frame * f = solver->frames.start + level;
    if (f->flipped) continue;
    int decision = f->decision;
    PUSH (solver->clause, -decision);
    POG ("%s adding literal %d",
      type (var (solver, decision)), -decision);
  }
  const int size = COUNT (solver->clause);
  POG ("found blocking clause of length %d", size);
  assert (solver->clause.start[0] == -lit);
  int other;
  for (;;) {
    other = TOP (solver->trail);
    Var * v = var (solver, other);
    const int decision = v->decision;
    POP (solver->trail);
    unassign (solver, other);
    if (decision) dec_level (solver);
    if (other == lit) break;
  }
  Clause * c = new_clause (solver->clause.start, size);
  assert (!c->glue), assert (!c->redundant);
  POGCLS (c, "blocking");
  add_clause_to_cnf (c, solver->cnf);
  if (size > 1) connect_clause (solver, c);
  // TODO check for subsumption for last learned clause(s)
  CLEAR (solver->clause);
  solver->next = COUNT (solver->trail);
  assign (solver, -lit, c);
  if (!solver->level) solver->iterating = 1;
}

static void flip (Primal * solver, int lit) {
  assert (val (solver, lit) > 0);
  Var * v = var (solver, lit);
  assert (v->decision == 1);
  v->decision = 2;
  POG ("%s flip %d", type (v), lit);
  POG ("%s assign %d", type (v), -lit);
  int n = COUNT (solver->trail) - 1;
  assert (PEEK (solver->trail, n) == lit);
  POKE (solver->trail, n, -lit);
  solver->next = n;
  v->val = -v->val;
  Frame * f = frame (solver, lit);
  assert (f->decision == lit);
  assert (!f->flipped);
  f->flipped = 1;
  assert (solver->flipped < solver->level);
  solver->flipped = solver->level;
  POG ("new flipped level %d", solver->flipped);
  if (!f->prev_flipped) solver->iterating = 1;
}

static int backtrack (Primal * solver) {
  if (!solver->level) return 0;
  stats.back.tracked++;
#ifndef NLOG
  int level = solver->level;
  while (PEEK (solver->frames, level).decision == 2)
    level--;
  POG ("backtrack %ld to level %d", stats.back.tracked, level);
#endif
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    const int decision = v->decision;
    if (decision == 1) {
      if (options.block && options.eager) block (solver, lit);
      else flip (solver, lit);
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
  POG ("%s seen literal %d", type (v), lit);
  assert (val (solver, lit) < 0);
  Frame * f = frame (solver, lit);
  if (!f->seen) {
    POG ("seen level %d", v->level);
    PUSH (solver->levels, v->level);
    f->seen = 1;
  }
  if (v->level == solver->level) return 1;
  POG ("%s adding literal %d", type (v), lit);
  PUSH (solver->clause, lit);
  return 0;
}

static int resolve_clause (Primal * solver, Clause * c) {
  assert (c);
  POGCLS (c, "resolving");
  c->recent = 1;
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
  stats.bumped++;
  POG ("%s bump variable %d", type (v), var2idx (solver, v));
  dequeue (solver, v);
  enqueue (solver, v);
}

static void bump_reason_clause_literals (Primal * solver, Clause * c) {
  if (!c) return;
  for (int i = 0; i < c->size; i++) {
    const int idx = abs (c->literals[i]);
    Var * v = var (solver, idx);
    if (v->seen) continue;
    if (!v->level) continue;
    POG ("also marking %s reason variable %d as seen", type (v), idx);
    PUSH (solver->seen, v);
    v->seen = 1;
  }
}

static void bump_all_reason_literals (Primal * solver) {
  for (Var ** p = solver->seen.start; p < solver->seen.top; p++)
    bump_reason_clause_literals (solver, (*p)->reason);
}

static void bump_seen (Primal * solver) {
  if (options.bump > 1) bump_all_reason_literals (solver);
  sort_seen (solver);
  for (Var ** p = solver->seen.start; p < solver->seen.top; p++)
    bump_variable (solver, *p);
}

static void reset_seen (Primal * solver) {
  while (!EMPTY (solver->seen))
    POP (solver->seen)->seen = 0;
}

static void update_ema (Primal * solver, double * ema,
                        double res, double n, double alpha) {
  assert (n > 0);
  *ema += MAX (1.0/n, alpha) * (res - *ema);
}

static int reset_levels (Primal * solver) {
  int res = COUNT (solver->levels);
  POG ("seen %d levels", res);
  while (!EMPTY (solver->levels)) {
    int level = POP (solver->levels);
    assert (0 <= level), assert (level < COUNT (solver->frames));
    Frame * f = solver->frames.start + level;
    assert (f->seen);
    f->seen = 0;
  }
  const long n = stats.conflicts;
  update_ema (solver, &solver->limit.restart.fast, res, n, 3e-2);
  update_ema (solver, &solver->limit.restart.slow, res, n, 1e-5);
  POG ("new fast moving glue average %.2f", solver->limit.restart.fast);
  POG ("new slow moving glue average %.2f", solver->limit.restart.slow);
  return res;
}

static int sort_clause (Primal * solver) {
  const int size = COUNT (solver->clause);
  int * lits = solver->clause.start;
  for (int i = 0; i < 2 && i < size; i++)
    for (int j = i + 1; j < size; j++)
      if (var (solver, lits[i])->level < var (solver, lits[j])->level) 
	SWAP (int, lits[i], lits[j]);
#ifndef NLOG
  if (size > 0) POG ("first sorted literal %d", lits[0]);
  if (size > 1) POG ("second sorted literal %d", lits[1]);
#endif
  return size;
}

static int jump_level (Primal * solver, int * lits, int size) {
  if (size < 2) return 0;
  assert (var (solver, lits[0])->level == solver->level);
  assert (var (solver, lits[1])->level < solver->level);
  return var (solver, lits[1])->level;
}

static Clause * learn_clause (Primal * solver, int glue) {
  if (!options.learn) return 0;
  sort_clause (solver);
  const int size = COUNT (solver->clause);
  const int level = jump_level (solver, solver->clause.start, size);
  POG ("jump level %d of size %d clause", level, size);
  assert (solver->flipped <= solver->level);
  if (solver->flipped > level) {
    POG ("flipped frame %d forces backtracking", solver->flipped);
    return 0;
  }
  stats.learned++;
  POG ("learning clause number %ld of size %d", stats.learned, size);
  Clause * res = new_clause (solver->clause.start, size);
  res->glue = glue;
  res->redundant = 1;
  res->recent = 1;
  POGCLS (res, "learned new");
  add_clause_to_cnf (res, solver->cnf);
  if (size > 1) connect_clause (solver, res);
  return res;
}

static int backjump (Primal * solver, Clause * c) {
  assert (c->size > 0);
  const int forced = c->literals[0];
  const int level = jump_level (solver, c->literals, c->size);
  stats.back.jumped++;
  POG ("backjump %ld, to level %d", stats.back.jumped, level);
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    if (v->level == level) break;
    (void) POP (solver->trail);
    const int decision = v->decision;
    unassign (solver, lit);
    if (decision) dec_level (solver);
  }
  assert (!val (solver, forced));
  assert (solver->level == level);
  solver->next = COUNT (solver->trail);
  assign (solver, forced, c);
  if (!level) solver->iterating = 1;
  return 1;
}

static int resolve_conflict (Primal * solver, Clause * conflict) {
  assert (EMPTY (solver->seen));
  assert (EMPTY (solver->clause));
  assert (EMPTY (solver->levels));
  Clause * c = conflict;
  const int * p = solver->trail.top;
  int unresolved = 0, uip = 0;
  for (;;) {
    unresolved += resolve_clause (solver, c);
    POG ("unresolved literals %d", unresolved);
    while (!var (solver, (uip = *--p))->seen)
      ;
    if (!--unresolved) break;
    POG ("%s resolving literal %d", type (var (solver, uip)), uip);
    c = var (solver, uip)->reason;
  }
  POG ("%s first UIP literal %d", type (var (solver, uip)), uip);
  PUSH (solver->clause, -uip);
  if (options.bump) bump_seen (solver);
  reset_seen (solver);
  return reset_levels (solver);
}

static int analyze (Primal * solver, Clause * conflict) {
  if (!solver->level) return 0;
  int glue = resolve_conflict (solver, conflict);
  Clause * c = learn_clause (solver, glue);
  CLEAR (solver->clause);
  if (c) return backjump (solver, c);
  stats.back.forced++;
  POG ("forced backtrack %ld", stats.back.forced);
  return backtrack (solver);
}

static int reducing (Primal * solver) {
  if (!options.reduce) return 0;
  if (!stats.conflicts &&
      solver->fixed > solver->limit.reduce.fixed) return 1;
  return stats.learned > solver->limit.reduce.learned;
}

static void inc_reduce_limit (Primal * solver) {
  if (!stats.conflicts) return;
  long inc = solver->limit.reduce.increment;
  POG ("reduce interval increment %d", inc);
  assert (inc > 0);
  solver->limit.reduce.interval += inc;
  POG ("new reduce interval %ld", solver->limit.reduce.interval);
  solver->limit.reduce.learned =
    stats.learned + solver->limit.reduce.interval;
  POG ("new reduce learned limit %ld", solver->limit.reduce.learned);
}

static int cmp_reduce (const void * p, const void * q) {
  Clause * c = *(Clause**) p, * d = *(Clause**) q;
  if (c->glue < d->glue) return 1;
  if (c->glue > d->glue) return -1;
  if (c->size < d->size) return 1;
  if (c->size > d->size) return -1;
  if (c->id < d->id) return 1;
  assert (c->id > d->id);
  return -1;
}

static int mark_satisfied_as_garbage (Primal * solver, Clause * c) {
  assert (!c->garbage);
  for (int i = 0; i < c->size; i++) {
    const int lit = c->literals[i];
    Var * v = var (solver, lit);
    if (v->level) continue;
    int tmp = v->val;
    if (!tmp) continue;
    if (lit < 0) tmp = -tmp;
    if (tmp < 0) continue;
    POGCLS (c, "root level satisfied by literal %d", lit);
    c->garbage = 1;
    return 1;
  }
  return 0;
}

static void reduce (Primal * solver) {
  stats.reductions++;
  POG ("reduction %ld", stats.reductions);
  Clauses candidates;
  INIT (candidates);
  CNF * cnf = solver->cnf;
  const int simplify = solver->fixed > solver->limit.reduce.fixed;
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++) {
    Clause * c = *p;
    if (c->garbage) continue;
    if (simplify && mark_satisfied_as_garbage (solver, c)) continue;
    if (c->active) continue;
    if (!c->redundant) continue;
    if (c->size <= options.keepsize) continue;
    if (c->glue <= options.keepglue) continue;
    const int recent = c->recent;
    c->recent = 0;
    if (recent) continue;
    PUSH (candidates, c);
  }
  solver->limit.reduce.fixed = solver->fixed;
  long n = COUNT (candidates);
  POG ("found %ld reduce candidates out of %ld", n, cnf->redundant);
  qsort (candidates.start, n, sizeof (Clause*), cmp_reduce);
  long target = n/2, marked = 0;
  POG ("target is to remove %ld clauses", target);
  for (long i = 0; i < target; i++) {
    Clause * c = PEEK (candidates, i);
    if (c->garbage) continue;
    POGCLS (c, "marking garbage");
    c->garbage = 1;
    marked++;
  }
  RELEASE (candidates);
  POG ("marked %ld clauses as garbage", marked);
  long flushed = 0;
  for (int idx = 1; idx <= solver->max_var; idx++)
    for (int sign = -1; sign <= 1; sign += 2) {
      int lit = sign * idx;
      Clauses * o = occs (solver, lit);
      Clause ** q = o->start, ** p = q;
      while (p < o->top) {
	Clause * c = *p++;
	if (c->garbage) flushed++;
	else *q++ = c;
      }
      o->top = q;
      if (EMPTY (*o)) RELEASE (*o);
    }
  POG ("flushed %ld occurrences to %ld garbage clauses", flushed, marked);
  collect_garbage_clauses (cnf);
  inc_reduce_limit (solver);
  report (solver, 1, '-');
}

static int restarting (Primal * solver) {
  if (!options.restart) return 0;
  if (solver->flipped) return 0;
  if (!solver->level) return 0;
  if (stats.conflicts < solver->limit.restart.conflicts) return 0;
  double limit = solver->limit.restart.slow * 1.1;
  int res = solver->limit.restart.fast > limit;
  if (!res) report (solver, 3, 'n');
  return res;
}

static void inc_restart_limit (Primal * solver) {
  const int inc = MAX (options.restartint, 1);
  solver->limit.restart.conflicts = stats.conflicts + inc;
  POG ("new restart conflicts limit %ld",
    solver->limit.restart.conflicts);
}

static int reuse_trail (Primal * solver) {
  if (!options.reuse) return 0;
  Var * next = next_decision (solver);
  int res = 0;
  for (res = 0; res < solver->level; res++) {
    Frame * f = solver->frames.start + res + 1;
    Var * v = var (solver, f->decision);
    assert (v->decision == 1);
    if (v->stamp < next->stamp) break;
  }
  POG ("reuse trail level %d", res);
  if (res) stats.reused++;
  return res;
}

static void restart (Primal * solver) {
  const int level = reuse_trail (solver);
  stats.restarts++;
  POG ("restart %d", stats.restarts);
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    if (v->level == level) break;
    const int decision = v->decision;
    assert (decision != 2);
    unassign (solver, lit);
    if (decision) dec_level (solver);
    (void) POP (solver->trail);
  }
  solver->next = COUNT (solver->trail);
  inc_restart_limit (solver);
  report (solver, 2, 'r');
}

static int rephasing (Primal * solver) {
  if (!options.rephase) return 0;
  return stats.conflicts >= solver->limit.rephase.conflicts;
}

static void inc_rephase_limit (Primal * solver) {
  solver->limit.rephase.conflicts *= 2;
  POG ("new rephase conflict limit %ld", solver->limit.rephase.conflicts);
}

static void rephase (Primal * solver) {
  stats.rephased++;
  solver->phase *= -1;
  LOG ("rephase %ld new phase %d", stats.rephased, solver->phase);
  for (int idx = 1; idx <= solver->max_var; idx++)
    var (solver, idx)->phase = solver->phase;
  inc_rephase_limit (solver);
  report (solver, 1, '~');
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
    else if (reducing (solver)) reduce (solver);
    else if (rephasing (solver)) rephase (solver);
    else if (restarting (solver)) restart (solver);
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
    } else if (reducing (solver)) reduce (solver);
    else if (rephasing (solver)) rephase (solver);
    else if (restarting (solver)) restart (solver);
    else decide (solver);
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
  if (bcp (solver)) return;
  if (satisfied (solver)) { print_model (solver, name); return; }
  for (;;) {
    Clause * conflict = bcp (solver);
    if (conflict) {
      if (!analyze (solver, conflict)) return;
    } else if (satisfied (solver)) {
      print_model (solver, name);
      if (!backtrack (solver)) return;
    } else if (reducing (solver)) reduce (solver);
    else if (restarting (solver)) restart (solver);
    else if (rephasing (solver)) rephase (solver);
    else decide (solver);
  }
}

int primal_deref (Primal * solver, int lit) {
  return val (solver, lit);
}

#ifndef NDEBUG
void print_trail (Primal * solver) {
  int prev = 0;
  for (const int * p = solver->trail.start; p < solver->trail.top; p++) {
    int lit = *p;
    Var * v = var (solver, lit);
    if (p > solver->trail.start) {
      if (v->level > prev) fputs (" | ", stdout);
      else fputc (' ', stdout);
    }
    prev = v->level;
    printf ("%d", lit);
    if (v->decision == 0) fputc ('p', stdout);
    if (v->decision == 1) fputc ('d', stdout);
    if (v->decision == 2) fputc ('f', stdout);
  }
  fputc ('\n', stdout);
}
#endif
