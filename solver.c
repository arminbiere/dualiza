#include "headers.h"

#ifdef NLOG
#define SOG(...) do { } while (0)
#define SOGCLS(...) do { } while (0)
#else
#define SOG(FMT,ARGS...) LOG ("%d " FMT, solver->level, ##ARGS)
#define SOGCLS(C,FMT,ARGS...) LOGCLS (C, "%d " FMT, solver->level, ##ARGS)
#endif

typedef struct Var Var;
typedef struct Queue Queue;
typedef struct Frame Frame;
typedef struct Limit Limit;
typedef enum VarType VarType;

typedef STACK (Var *) VarStack;
typedef STACK (Frame) FrameStack;

enum VarType {
  INPUT_VARIABLE = 0,
  PRIMAL_VARIABLE = 1,
  DUAL_VARIABLE = 2,
};

struct Var {
  VarType type;
  char decision, seen;
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
  int prev_flipped_level;
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
  long subsumed;
};

struct Solver {
  Var * vars;
  char iterating, dual;
  int max_var, next, level, fixed, phase;
  int last_flipped_level, num_flipped_levels;
  int unassigned_primal_and_input_variables;
  IntStack trail, clause, levels, * sorted;
  FrameStack frames;
  VarStack seen;
  Limit limit;
  struct { CNF * primal, * dual; } cnf;
  struct { Queue primal, input, dual; } queue;
};

static Var * var (Solver * solver, int lit) {
  assert (lit);
  int idx = abs (lit);
  assert (idx <= solver->max_var);
  return solver->vars + idx;
}

#ifndef NLOG

static int var2idx (Solver * solver, Var * v) {
  assert (solver->vars < v);
  assert (v <= solver->vars + solver->max_var);
  return (int)(long)(v - solver->vars);
}

#endif

static int val (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  int res = v->val;
  if (lit < 0) res = -res;
  return res;
}

static Queue * queue (Solver * solver, Var * v) {
  if (v->type == DUAL_VARIABLE) return &solver->queue.dual;
  if (!options.inputs) return &solver->queue.input;
  if (v->type == PRIMAL_VARIABLE) return &solver->queue.primal;
  return &solver->queue.input;
}

#ifndef NLOG

static const char * var_type (Var * v) {
  assert (v);
  if (v->type == PRIMAL_VARIABLE) return "primal";
  if (v->type == INPUT_VARIABLE) return "input";
  assert (v->type == DUAL_VARIABLE);
  return "dual";
}

static const char * queue_type (Solver * s, Queue * q) {
  if (q == &s->queue.primal) return "primal";
  if (q == &s->queue.input) return "input";
  assert (q == &s->queue.dual);
  return "dual";
}

#endif

static void update_queue (Solver * solver, Queue * q, Var * v) {
  assert (!v || q == queue (solver, v));
  q->search = v;
#ifndef NLOG
  if (v) {
    SOG ("updating %s variable %d in %s queue",
      var_type (v), (int)(long)(v - solver->vars), queue_type (solver, q));
  } else SOG ("flushed %s queue", queue_type (solver, q));
#endif
}

static void enqueue (Solver * solver, Var * v) {
  Queue * q = queue (solver, v);
  assert (!v->next);
  assert (!v->prev);
  v->stamp = ++q->stamp;
  SOG ("%s enqueue variable %d stamp %ld",
    var_type (v), var2idx (solver, v), v->stamp);
  if (!q->first) q->first = v;
  if (q->last) q->last->next = v;
  v->prev = q->last;
  q->last = v;
  if (!v->val) update_queue (solver, q, v);
}

static void dequeue (Solver * solver, Var * v) {
  Queue * q = queue (solver, v);
  SOG ("%s dequeue variable %d stamp %ld",
    var_type (v), var2idx (solver, v), v->stamp);
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

static Frame new_frame (Solver * solver, int decision) {
  Frame res = { decision, 0, 0, solver->last_flipped_level };
  return res;
}

static Frame * frame (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  assert (v->level < COUNT (solver->frames));
  return solver->frames.start + v->level;
}

static void init_reduce_limit (Solver * solver) {
  solver->limit.reduce.learned = MAX (options.reduceinit, 0);
  solver->limit.reduce.interval = MAX (options.reduceinit, 0);
  solver->limit.reduce.increment = MAX (options.reduceinc, 1);
  LOG ("initial reduce interval %ld", solver->limit.reduce.interval);
  LOG ("initial reduce increment %ld", solver->limit.reduce.increment);
  LOG ("initial reduce learned limit %ld",
    solver->limit.reduce.learned);
}

static void init_restart_limit (Solver * solver) {
  solver->limit.restart.conflicts = MAX (options.restartint, 1);
  LOG ("initial restart conflict limit %ld",
    solver->limit.restart.conflicts);
}

static void set_subsumed_limit (Solver * solver) {
  solver->limit.subsumed += COUNT (solver->cnf.primal->clauses)/10 + 1000;
  LOG ("subsumed limit %ld", solver->limit.subsumed);
}

static void init_limits (Solver * solver) {
  init_reduce_limit (solver);
  init_restart_limit (solver);
  set_subsumed_limit (solver);
}

Solver * new_solver (CNF * primal, IntStack * inputs, CNF * dual) {
  assert (primal);
  Solver * solver;
  NEW (solver);
  solver->cnf.primal = primal;
  if (dual) {
    LOG (
      "new solver over %ld primal and %ld dual clauses and %ld inputs",
      (long) COUNT (primal->clauses),
      (long) COUNT (dual->clauses),
      (long) COUNT (*inputs));
    solver->cnf.dual = dual;
    solver->dual = 1;
  } else {
    LOG (
      "new solver over %ld clauses and %ld inputs",
      (long) COUNT (primal->clauses),
      (long) COUNT (*inputs));
    solver->cnf.dual = 0;
    solver->dual = 0;
  }
  solver->sorted = inputs;
  int max_input_var = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (0 < input);
    if (input > max_input_var) max_input_var = input;
  }
  LOG ("maximum variable index %d in inputs", max_input_var);
  assert (COUNT (*inputs) == max_input_var);
  int max_var = max_input_var;
  int max_primal_var = maximum_variable_index (primal);
  LOG ("maximum variable index %d in primal CNF", max_primal_var);
  if (max_var <  max_primal_var) {
    LOG ("%d primal gate variables %d ... %d",
      max_primal_var - max_var, max_var + 1, max_primal_var);
    max_var = max_primal_var;
  } else LOG ("no primal gate variables");
  if (dual) {
    int max_dual_var = dual ? maximum_variable_index (dual) : 0;
    LOG ("maximum variable index %d in dual CNF", max_dual_var);
    if (max_var <  max_dual_var) {
      LOG ("%d dual gate variables %d ... %d",
        max_dual_var - max_var, max_var + 1, max_dual_var);
      max_var = max_dual_var;
    } else LOG ("no dual gate variables");
    int min_non_input_var_in_dual =
      minimum_variable_index_above (dual, max_input_var);
    LOG ("minimum variable index %d above %d in dual CNF",
      min_non_input_var_in_dual, max_input_var);
#ifndef NDEBUG
    assert (min_non_input_var_in_dual > max_primal_var);
    if (min_non_input_var_in_dual < INT_MAX)
      assert (min_non_input_var_in_dual <= max_dual_var);
#else
    (void) min_non_input_var_in_dual;
#endif
  }
  solver->max_var = max_var;
  LOG ("maximum variable index %d", solver->max_var);
  solver->unassigned_primal_and_input_variables =
    MAX (max_primal_var, max_input_var);
  LOG ("%d unassigned primal gate or input variables %d",
    solver->unassigned_primal_and_input_variables);
  ALLOC (solver->vars, solver->max_var + 1);
  for (int idx = 1; idx <= solver->max_var; idx++)
    solver->vars[idx].phase = solver->phase;
  assert (max_primal_var <= max_var);
  for (int idx = 1; idx <= max_input_var; idx++) {
    Var * v = var (solver, idx);
    assert (!v->type);
    LOG ("input variable %d", idx);
    v->type = INPUT_VARIABLE;
  }
  for (int idx = max_input_var + 1; idx <= max_primal_var; idx++) {
    Var * v = var (solver, idx);
    assert (!v->type);
    LOG ("primal variable %d", idx);
    v->type = PRIMAL_VARIABLE;
  }
  if (dual) {
    for (int idx = max_primal_var + 1; idx <= max_var; idx++) {
      Var * v = var (solver, idx);
      assert (!v->type);
      LOG ("dual variable %d", idx);
      v->type = DUAL_VARIABLE;
    }
  }
  LOG ("connecting variables");
  for (int idx = 1; idx <= solver->max_var; idx++)
    enqueue (solver, var (solver, idx));
  solver->phase = options.phaseinit ? 1 : -1;
  LOG ("default initial phase %d", solver->phase);
  init_limits (solver);
  PUSH (solver->frames, new_frame (solver, 0));
  assert (COUNT (solver->frames) == solver->level + 1);
  return solver;
}

void delete_solver (Solver * solver) {
  LOG ("deleting solver");
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

static CNF * cnf (Solver * solver, Clause * c) {
  return c->dual ? solver->cnf.dual : solver->cnf.primal;
}

static void assign (Solver * solver, int lit, Clause * reason) {
  int idx = abs (lit);
  assert (0 < idx), assert (idx <= solver->max_var);
  Var * v = solver->vars + idx;
  if (!reason) SOG ("%s assign %d decision", var_type (v), lit); 
  else SOGCLS (reason, "%s assign %d reason", var_type (v), lit);
  assert (!v->val);
  if (lit < 0) v->val = v->phase = -1;
  else v->val = v->phase = 1;
  v->level = solver->level;
  if (v->level) {
    v->reason = reason;
    if (reason)
      mark_clause_active (v->reason, cnf (solver, v->reason));
  } else {
    solver->fixed++;
    v->reason = 0;
  }
  PUSH (solver->trail, lit);
  if (v->type != DUAL_VARIABLE) {
    assert (solver->unassigned_primal_and_input_variables > 0);
    solver->unassigned_primal_and_input_variables--;
  }
}

static Clauses * occs (Solver * solver, int lit) {
  return &var (solver, lit)->occs[lit < 0];
}

static void connect_literal (Solver * solver, Clause * c, int lit) {
  SOGCLS (c, "connecting literal %d to", lit);
  Clauses * cs = occs (solver, lit);
  PUSH (*cs, c);
}

static void connect_clause (Solver * solver, Clause * c) {
  assert (c->size > 1);
  connect_literal (solver, c, c->literals[0]);
  connect_literal (solver, c, c->literals[1]);
}

static int connect_cnf (Solver * solver) {
  assert (!solver->level);
  Clauses * clauses = &solver->cnf.primal->clauses;
  LOG ("connecting %ld clauses to Solver solver", (long) COUNT (*clauses));
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

static int remaining_variables (Solver * solver) {
  return solver->max_var - solver->fixed;
}

static void primal_header (Solver * solver) {
  assert (!solver->dual);
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

static void
primal_report (Solver * solver, int verbosity, const char type) {
  assert (!solver->dual);
  if (options.verbosity < verbosity) return;
  if (!(stats.reports++ % 20)) primal_header (solver);
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
    solver->cnf.primal->redundant,
    solver->cnf.primal->irredundant,
    remaining_variables (solver));
}

static void dual_header (Solver * solver) {
  assert (solver->dual);
  msg (1, "");
  msg (1,
    "  "
    "    time"
    "  memory"
    " conflicts"
    "  learned"
    "    primal"
    "      dual"
    " variables");
  msg (1, "");
}

static void
dual_report (Solver * solver, int verbosity, const char type) {
  assert (solver->dual);
  if (options.verbosity < verbosity) return;
  if (!(stats.reports++ % 20)) dual_header (solver);
  assert (solver->cnf.dual);
  assert (!solver->cnf.dual->redundant);
  msg (1,
    "%c"
    " %8.2f"
    " %7.1f"
    " %9ld"
    " %8ld"
    " %9ld"
    " %9ld"
    " %8ld",
    type,
    process_time (),
    current_resident_set_size ()/(double)(1<<20),
    stats.conflicts,
    solver->cnf.primal->redundant,
    solver->cnf.primal->irredundant,
    solver->cnf.dual->irredundant,
    remaining_variables (solver));
}

static void report (Solver * solver, int verbosity, const char type) {
  if (solver->dual) dual_report (solver, verbosity, type);
  else primal_report (solver, verbosity, type);
}

static Clause * primal_propagate (Solver * solver) {
  Clause * res = 0;
  while (!res && solver->next < COUNT (solver->trail)) {
    int lit = solver->trail.start[solver->next++];
    assert (val (solver, lit) > 0);
    SOG ("primal propagating %d", lit);
    stats.propagated++;
    Clauses * o = occs (solver, -lit);
    Clause ** q = o->start, ** p = q;
    while (!res && p < o->top) {
      Clause * c = *q++ = *p++;
      if (c->garbage) { q--; continue; }
      SOGCLS (c, "visiting while primal propagating %d", lit);
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
	SOGCLS (c, "disconnecting literal %d from", -lit);
	c->literals[0] = replacement;
	c->literals[i] = -lit;
	connect_literal (solver, c, replacement);
	q--;
      } else if (!other_val) {
	SOGCLS (c, "forcing %d", other);
	assign (solver, other, c);
      } else {
	assert (other_val < 0);
	stats.conflicts++;
	SOGCLS (c, "conflict %ld", stats.conflicts);
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

static int satisfied (Solver * solver) {
  return solver->next == solver->max_var;
}

static void inc_level (Solver * solver) {
  solver->level++;
  SOG ("incremented decision level");
}

static void dec_level (Solver * solver) {
  assert (solver->level > 0);
  solver->level--;
  SOG ("decremented decision level");
  Frame f = POP (solver->frames);
  SOG ("popped %s frame", f.flipped ? "flipped" : "decision");
  if (f.prev_flipped_level != solver->last_flipped_level) {
    assert (f.flipped);
    assert (f.prev_flipped_level < solver->last_flipped_level);
    assert (solver->last_flipped_level == solver->level + 1);
    solver->last_flipped_level = f.prev_flipped_level;
    SOG ("restored flipped level %d", solver->last_flipped_level);
    assert (solver->num_flipped_levels);
    solver->num_flipped_levels--;
  } else assert (!f.flipped);
  assert (COUNT (solver->frames) == solver->level + 1);
}

static Var * next_queue (Solver * solver, Queue * queue) {
  Var * res = queue->search;
  while (res && res->val) 
    res = res->prev, stats.searched++;
  update_queue (solver, queue, res);
  return res;
}

static Var * next_decision (Solver * solver) {
  Var * v = next_queue (solver, &solver->queue.input);
  if (!v) v = next_queue (solver, &solver->queue.primal);
  if (!v) v = next_queue (solver, &solver->queue.dual);
  assert (v);
  SOG ("next %s decision %d stamped %ld",
    var_type (v), var2idx (solver, v), v->stamp);
  return v;
}

static void decide (Solver * solver) {
  Var * v = next_decision (solver);
  int lit = v - solver->vars;
  if (v->phase < 0) lit = -lit;
  inc_level (solver);
  SOG ("%s decide %d", var_type (v), lit);
  assign (solver, lit, 0);
  assert (!v->decision);
  v->decision = 1;
  stats.decisions++;
  PUSH (solver->frames, new_frame (solver, lit));
  assert (COUNT (solver->frames) == solver->level + 1);
}

static void unassign (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  assert (solver->level == v->level);
  SOG ("%s unassign %d", var_type (v), lit);
  assert (v->val);
  v->val = v->decision = 0;
  if (v->reason)
    mark_clause_inactive (v->reason, cnf (solver, v->reason));
  Queue * q = queue (solver, v);
  if (!q->search || q->search->stamp < v->stamp)
    update_queue (solver, q, v);
  if (v->type != DUAL_VARIABLE) {
    solver->unassigned_primal_and_input_variables++;
    assert (solver->unassigned_primal_and_input_variables > 0);
  }
}

static int lit_sign (int literal) { return literal < 0 ? -1 : 1; }

static void mark_literal (Solver * solver, int lit) {
  SOG ("marking literal %d", lit);
  Var * v = var (solver, lit);
  assert (!v->seen);
  v->seen = lit_sign (lit);
}

static void unmark_literal (Solver * solver, int lit) {
  SOG ("unmarking literal %d", lit);
  Var * v = var (solver, lit);
  assert (v->seen == lit_sign (lit));
  v->seen = 0;
}

static void mark_clause (Solver * solver, Clause * c) {
  for (int i = 0; i < c->size; i++)
    mark_literal (solver, c->literals[i]);
}

static void unmark_clause (Solver * solver, Clause * c) {
  for (int i = 0; i < c->size; i++)
    unmark_literal (solver, c->literals[i]);
}

static int marked_literal (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  return v->seen == lit_sign (lit);
}

static int subsumed (Solver * solver, Clause * c, int expected) {
  if (c->garbage) return 0;
  if (c->size < expected) return 0;
  int slack = c->size - expected;
  for (int i = 0; i < c->size; i++) {
    int lit = c->literals[i];
    if (!marked_literal (solver, lit)) {
      if (!slack--) return 0;
    } else if (!--expected) return 1;
  }
  return 0;
}

static void flush_garbage_occurrences (Solver * solver) {
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
  SOG ("flushed %ld garbage occurrences", flushed);
}

static void subsume (Solver * solver, Clause * c) {
  assert (!c->redundant);
  mark_clause (solver, c);
  int limit = MAX (options.subsumelimit, 0);
  assert (!c->dual);
  Clause ** p = solver->cnf.primal->clauses.top;
  while (p != solver->cnf.primal->clauses.start) {
    Clause * d = *--p;
    if (d == c) continue;
    if (d->garbage) break;
    if (subsumed (solver, d, c->size)) {
      SOGCLS (d, "subsumed");
      d->garbage = 1;
      stats.subsumed++;
    } else if (!limit--) break;
  }
  unmark_clause (solver, c);
  if (stats.subsumed <= solver->limit.subsumed) return;
  flush_garbage_occurrences (solver);
  collect_garbage_clauses (solver->cnf.primal);
  set_subsumed_limit (solver);
  report (solver, 1, '/');
}

static void block_clause (Solver * solver, int lit) {
  assert (solver->level > 0);
  stats.blocked.clauses++;
  SOG ("adding blocking clause for decision %d", lit);
  assert (val (solver, lit) > 0);
  assert (var (solver, lit)->decision == 1);
  assert (EMPTY (solver->clause));
  for (int level = solver->level; level > 0; level--) {
    Frame * f = solver->frames.start + level;
    if (f->flipped) continue;
    int decision = f->decision;
    PUSH (solver->clause, -decision);
    SOG ("%s adding literal %d",
      var_type (var (solver, decision)), -decision);
  }
  const int size = COUNT (solver->clause);
  stats.blocked.literals += size;
  SOG ("found blocking clause of length %d", size);
  assert (solver->clause.start[0] == -lit);
  int other;
  for (;;) {
    other = TOP (solver->trail);
    Var * v = var (solver, other);
    const int decision = v->decision;
    (void) POP (solver->trail);
    unassign (solver, other);
    if (decision) dec_level (solver);
    if (other == lit) break;
  }
  Clause * c = new_clause (solver->clause.start, size);
  assert (!c->glue), assert (!c->redundant);
  SOGCLS (c, "blocking");
  add_clause_to_cnf (c, solver->cnf.primal);
  if (size > 1) connect_clause (solver, c);
  CLEAR (solver->clause);
  solver->next = COUNT (solver->trail);
  assign (solver, -lit, c);
  if (!solver->level) solver->iterating = 1;
  if (options.subsume) subsume (solver, c);
}

static void flip (Solver * solver, int lit) {
  assert (val (solver, lit) > 0);
  Var * v = var (solver, lit);
  assert (v->decision == 1);
  v->decision = 2;
  SOG ("%s flip %d", var_type (v), lit);
  SOG ("%s assign %d", var_type (v), -lit);
  int n = COUNT (solver->trail) - 1;
  assert (PEEK (solver->trail, n) == lit);
  POKE (solver->trail, n, -lit);
  solver->next = n;
  v->val = -v->val;
  Frame * f = frame (solver, lit);
  assert (f->decision == lit);
  assert (!f->flipped);
  f->flipped = 1;
  assert (solver->last_flipped_level < solver->level);
  solver->last_flipped_level = solver->level;
  solver->num_flipped_levels++;
  SOG ("new flipped level %d number %d",
    solver->last_flipped_level, solver->num_flipped_levels);
  if (solver->num_flipped_levels == solver->level)
    solver->iterating = 1;
}

static int blocking (Solver * solver) {
  if (!options.block) return 0;
  assert (solver->level >= solver->num_flipped_levels);
  const int size = solver->level - solver->num_flipped_levels;
  SOG ("expected blocking clause size %d", size);
  return size <= options.blocklimit;
}

static int backtrack (Solver * solver) {
  if (!solver->level) return 0;
  stats.back.tracked++;
#ifndef NLOG
  int level = solver->level;
  while (PEEK (solver->frames, level).decision == 2)
    level--;
  SOG ("backtrack %ld to level %d", stats.back.tracked, level);
#endif
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    const int decision = v->decision;
    if (decision == 1) {
      if (blocking (solver)) block_clause (solver, lit);
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

static int resolve_literal (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  if (!v->level) return 0;
  if (v->seen) return 0;
  v->seen = 1;
  PUSH (solver->seen, v);
  SOG ("%s seen literal %d", var_type (v), lit);
  assert (val (solver, lit) < 0);
  Frame * f = frame (solver, lit);
  if (!f->seen) {
    SOG ("seen level %d", v->level);
    PUSH (solver->levels, v->level);
    f->seen = 1;
  }
  if (v->level == solver->level) return 1;
  SOG ("%s adding literal %d", var_type (v), lit);
  PUSH (solver->clause, lit);
  return 0;
}

static int resolve_clause (Solver * solver, Clause * c) {
  assert (c);
  SOGCLS (c, "resolving");
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

static void sort_seen (Solver * solver) {
  const int n = COUNT (solver->seen);
  qsort (solver->seen.start, n, sizeof *solver->seen.start, cmp_seen);
}

static void bump_variable (Solver * solver, Var * v) {
  stats.bumped++;
  SOG ("%s bump variable %d", var_type (v), var2idx (solver, v));
  dequeue (solver, v);
  enqueue (solver, v);
}

static void bump_reason_clause_literals (Solver * solver, Clause * c) {
  if (!c) return;
  for (int i = 0; i < c->size; i++) {
    const int idx = abs (c->literals[i]);
    Var * v = var (solver, idx);
    if (v->seen) continue;
    if (!v->level) continue;
    SOG ("also marking %s reason variable %d as seen", var_type (v), idx);
    PUSH (solver->seen, v);
    v->seen = 1;
  }
}

static void bump_all_reason_literals (Solver * solver) {
  for (Var ** p = solver->seen.start; p < solver->seen.top; p++)
    bump_reason_clause_literals (solver, (*p)->reason);
}

static void bump_seen (Solver * solver) {
  if (options.bump > 1) bump_all_reason_literals (solver);
  sort_seen (solver);
  for (Var ** p = solver->seen.start; p < solver->seen.top; p++)
    bump_variable (solver, *p);
}

static void reset_seen (Solver * solver) {
  while (!EMPTY (solver->seen))
    POP (solver->seen)->seen = 0;
}

static void update_ema (Solver * solver, double * ema,
                        double res, double n, double alpha) {
  assert (n > 0);
  *ema += MAX (1.0/n, alpha) * (res - *ema);
}

static int reset_levels (Solver * solver) {
  int res = COUNT (solver->levels);
  SOG ("seen %d levels", res);
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
  SOG ("new fast moving glue average %.2f", solver->limit.restart.fast);
  SOG ("new slow moving glue average %.2f", solver->limit.restart.slow);
  return res;
}

static int sort_clause (Solver * solver) {
  const int size = COUNT (solver->clause);
  int * lits = solver->clause.start;
  for (int i = 0; i < 2 && i < size; i++)
    for (int j = i + 1; j < size; j++)
      if (var (solver, lits[i])->level < var (solver, lits[j])->level) 
	SWAP (int, lits[i], lits[j]);
#ifndef NLOG
  if (size > 0) SOG ("first sorted literal %d", lits[0]);
  if (size > 1) SOG ("second sorted literal %d", lits[1]);
#endif
  return size;
}

static int jump_level (Solver * solver, int * lits, int size) {
  if (size < 2) return 0;
  assert (var (solver, lits[0])->level == solver->level);
  assert (var (solver, lits[1])->level < solver->level);
  return var (solver, lits[1])->level;
}

static Clause * learn_clause (Solver * solver, int glue) {
  if (!options.learn) return 0;
  sort_clause (solver);
  const int size = COUNT (solver->clause);
  const int level = jump_level (solver, solver->clause.start, size);
  SOG ("jump level %d of size %d clause", level, size);
  assert (solver->last_flipped_level <= solver->level);
  if (solver->last_flipped_level > level) {
    SOG ("flipped frame %d forces backtracking",
      solver->last_flipped_level);
    return 0;
  }
  stats.learned++;
  SOG ("learning clause number %ld of size %d", stats.learned, size);
  Clause * res = new_clause (solver->clause.start, size);
  res->glue = glue;
  res->redundant = 1;
  res->recent = 1;
  SOGCLS (res, "learned new");
  add_clause_to_cnf (res, solver->cnf.primal);
  if (size > 1) connect_clause (solver, res);
  return res;
}

static int back_jump (Solver * solver, Clause * c) {
  assert (c->size > 0);
  const int forced = c->literals[0];
  const int level = jump_level (solver, c->literals, c->size);
  stats.back.jumped++;
  SOG ("back jump %ld to level %d", stats.back.jumped, level);
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

static int resolve_conflict (Solver * solver, Clause * conflict) {
  assert (EMPTY (solver->seen));
  assert (EMPTY (solver->clause));
  assert (EMPTY (solver->levels));
  Clause * c = conflict;
  const int * p = solver->trail.top;
  int unresolved = 0, uip = 0;
  for (;;) {
    unresolved += resolve_clause (solver, c);
    SOG ("unresolved literals %d", unresolved);
    while (!var (solver, (uip = *--p))->seen)
      ;
    if (!--unresolved) break;
    SOG ("%s resolving literal %d", var_type (var (solver, uip)), uip);
    c = var (solver, uip)->reason;
  }
  SOG ("%s first UIP literal %d", var_type (var (solver, uip)), uip);
  PUSH (solver->clause, -uip);
  if (options.bump) bump_seen (solver);
  reset_seen (solver);
  return reset_levels (solver);
}

static int analyze (Solver * solver, Clause * conflict) {
  if (!solver->level) return 0;
  int glue = resolve_conflict (solver, conflict);
  Clause * c = learn_clause (solver, glue);
  CLEAR (solver->clause);
  if (c) return back_jump (solver, c);
  stats.back.forced++;
  SOG ("forced backtrack %ld", stats.back.forced);
  return backtrack (solver);
}

static int reducing (Solver * solver) {
  if (!options.reduce) return 0;
  if (!stats.conflicts &&
      solver->fixed > solver->limit.reduce.fixed) return 1;
  return stats.learned > solver->limit.reduce.learned;
}

static void inc_reduce_limit (Solver * solver) {
  if (!stats.conflicts) return;
  long inc = solver->limit.reduce.increment;
  SOG ("reduce interval increment %d", inc);
  assert (inc > 0);
  solver->limit.reduce.interval += inc;
  SOG ("new reduce interval %ld", solver->limit.reduce.interval);
  solver->limit.reduce.learned =
    stats.learned + solver->limit.reduce.interval;
  SOG ("new reduce learned limit %ld", solver->limit.reduce.learned);
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

static int mark_satisfied_as_garbage (Solver * solver, Clause * c) {
  assert (!c->garbage);
  for (int i = 0; i < c->size; i++) {
    const int lit = c->literals[i];
    Var * v = var (solver, lit);
    if (v->level) continue;
    int tmp = v->val;
    if (!tmp) continue;
    if (lit < 0) tmp = -tmp;
    if (tmp < 0) continue;
    SOGCLS (c, "root level satisfied by literal %d", lit);
    c->garbage = 1;
    return 1;
  }
  return 0;
}

static void reduce (Solver * solver) {
  stats.reductions++;
  SOG ("reduction %ld", stats.reductions);
  Clauses candidates;
  INIT (candidates);
  CNF * primal = solver->cnf.primal;
  const int simplify = solver->fixed > solver->limit.reduce.fixed;
  for (Clause ** p = primal->clauses.start; p < primal->clauses.top; p++) {
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
  SOG ("found %ld reduce candidates out of %ld", n, primal->redundant);
  qsort (candidates.start, n, sizeof (Clause*), cmp_reduce);
  long target = n/2, marked = 0;
  SOG ("target is to remove %ld clauses", target);
  for (long i = 0; i < target; i++) {
    Clause * c = PEEK (candidates, i);
    if (c->garbage) continue;
    SOGCLS (c, "marking garbage");
    c->garbage = 1;
    marked++;
  }
  RELEASE (candidates);
  SOG ("marked %ld clauses as garbage", marked);
  flush_garbage_occurrences (solver);
  collect_garbage_clauses (primal);
  inc_reduce_limit (solver);
  report (solver, 1, '-');
}

static void inc_restart_limit (Solver * solver) {
  const int inc = MAX (options.restartint, 1);
  solver->limit.restart.conflicts = stats.conflicts + inc;
  SOG ("new restart conflicts limit %ld",
    solver->limit.restart.conflicts);
}

static int restarting (Solver * solver) {
  if (!options.restart) return 0;
  if (solver->last_flipped_level) return 0;
  if (!solver->level) return 0;
  if (stats.conflicts <= solver->limit.restart.conflicts) return 0;
  inc_restart_limit (solver);
  double limit = solver->limit.restart.slow * 1.1;
  int res = solver->limit.restart.fast > limit;
  if (!res) report (solver, 3, 'n');
  return res;
}

static int reuse_trail (Solver * solver) {
  if (!options.reuse) return 0;
  Var * next = next_decision (solver);
  int res = 0;
  for (res = 0; res < solver->level; res++) {
    Frame * f = solver->frames.start + res + 1;
    Var * v = var (solver, f->decision);
    assert (v->decision == 1);
    if (v->stamp < next->stamp) break;
  }
  SOG ("reuse trail level %d", res);
  if (res) stats.reused++;
  return res;
}

static void restart (Solver * solver) {
  const int level = reuse_trail (solver);
  stats.restarts++;
  SOG ("restart %d", stats.restarts);
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
  report (solver, 2, 'r');
}

int primal_sat (Solver * solver) {
  if (!connect_cnf (solver)) return 20;
  if (primal_propagate (solver)) return 20;
  if (satisfied (solver)) return 10;
  int res = 0;
  while (!res) {
    Clause * conflict = primal_propagate (solver);
    if (conflict) {
       if (!analyze (solver, conflict)) res = 20;
    } else if (satisfied (solver)) res = 10;
    else if (reducing (solver)) reduce (solver);
    else if (restarting (solver)) restart (solver);
    else decide (solver);
  }
  return res;
}

void primal_count (Number res, Solver * solver) {
  if (!connect_cnf (solver)) return;
  if (primal_propagate (solver)) return;
  if (satisfied (solver)) {
    SOG ("single root level model");
    inc_number (res);
    return;
  }
  for (;;) {
    Clause * conflict = primal_propagate (solver);
    if (conflict) {
      if (!analyze (solver, conflict)) return;
    } else if (satisfied (solver)) {
      SOG ("new model");
      inc_number (res);
      stats.models++;
      if (!backtrack (solver)) return;
    } else if (reducing (solver)) reduce (solver);
    else if (restarting (solver)) restart (solver);
    else decide (solver);
  }
}

void dual_count (Number res, Solver * solver) {
  assert (solver->dual);
  return primal_count (res, solver);
}

static void print_model (Solver * solver, Name name) {
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

void primal_enumerate (Solver * solver, Name name) {
  if (!connect_cnf (solver)) return;
  if (primal_propagate (solver)) return;
  if (satisfied (solver)) { print_model (solver, name); return; }
  for (;;) {
    Clause * conflict = primal_propagate (solver);
    if (conflict) {
      if (!analyze (solver, conflict)) return;
    } else if (satisfied (solver)) {
      print_model (solver, name);
      stats.models++;
      if (!backtrack (solver)) return;
    } else if (reducing (solver)) reduce (solver);
    else if (restarting (solver)) restart (solver);
    else decide (solver);
  }
}

int deref (Solver * solver, int lit) {
  return val (solver, lit);
}

#ifndef NDEBUG
void print_trail (Solver * solver) {
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
