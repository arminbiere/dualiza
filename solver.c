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
typedef enum DecisionType DecisionType;
typedef enum Mode Mode;

typedef STACK (Var *) VarStack;
typedef STACK (Frame) FrameStack;

enum VarType {
  RELEVANT_VARIABLE = 0,
  IRRELEVANT_VARIABLE = 1,
  PRIMAL_VARIABLE = 2,
  DUAL_VARIABLE = 3,
};

enum DecisionType {
  UNDECIDED = 0,
  DECISION = 1,
  FLIPPED = 2,
};

struct Var {
  VarType type;
  signed int val : 2;		// -1 = false, 0 = unassigned, 1 = true
  signed int phase : 2;		// saved value of previous assignment
  signed int first : 2;		// value in first model
  unsigned decision : 2;	// actually of type 'DecisionType'
  signed int seen : 2;		// signed mark flag
  int level;			// decision level
  Clause * reason;		// reason clause
  Var * prev, * next;		// doubly linked list in VMTF queue
  long stamp;			// VMTF queue enqueue time stamp
};

struct Queue {			// VMTF decision queue
  Var * first, * last;
  Var * search;			// searched up to this variable
};

struct Frame {
  char seen;			// seen during conflict analysis
  char flipped;			// already flipped
  int decision;			// current decision literal
  int counted;			// number of counted unassigned variables
  int prev;			// prev decision or flipped level
};

struct Limit {
  struct {
    long learned, interval, increment;
    int primal_or_input_fixed;
  } reduce;
  struct {
    long conflicts;
    double slow, fast;
  } restart;
  long subsumed;
  struct { long count, report, log2report; } models;
  struct { Number report; long log2report; } count;
};

struct Solver {
  Var * vars;
  char found_new_fixed_variable;
  char dual_solving_enabled;
  char model_printing_enabled;
  char split_on_relevant_first;
  int max_var, max_lit;
  int max_input_var, max_primal_or_input_var;
  int level, phase;
  struct { int primal, dual; } next;
  int last_flipped_level, num_flipped_levels;
  int last_decision_level, num_decision_levels;
  int unassigned_primal_and_input_variables;
  int unassigned_input_variables;
  int primal_or_input_fixed;
  int dual_non_input_fixed;
  IntStack trail, clause, levels, * sorted;
  FrameStack frames;
  VarStack seen;
  Limit limit;
  struct { CNF * primal, * dual; } cnf;
  struct {
    long stamp;
    Queue relevant, irrelevant, primal, dual;
  } queue;
  struct { Clauses * primal, * dual; } occs;
  Number count;
  Name name;
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

static int level (Solver * solver, int lit) {
  assert (val (solver, lit));
  return var (solver, lit)->level;
}

static Queue * queue (Solver * solver, Var * v) {
  if (v->type == RELEVANT_VARIABLE) return &solver->queue.relevant;
  if (v->type == IRRELEVANT_VARIABLE) return &solver->queue.irrelevant;
  if (v->type == PRIMAL_VARIABLE) return &solver->queue.primal;
  assert (v->type == DUAL_VARIABLE);
  return &solver->queue.dual;
}

#ifndef NLOG

static const char * var_type (Var * v) {
  assert (v);
  if (v->type == PRIMAL_VARIABLE) return "primal";
  if (v->type == RELEVANT_VARIABLE ) return "relevant";
  if (v->type == IRRELEVANT_VARIABLE) return "irrelevant";
  assert (v->type == DUAL_VARIABLE);
  return "dual";
}

static const char * queue_type (Solver * s, Queue * q) {
  if (q == &s->queue.relevant) return "relevant";
  if (q == &s->queue.irrelevant) return "irrelevant";
  if (q == &s->queue.primal) return "primal";
  assert (q == &s->queue.dual);
  return "dual";
}

#endif

static void update_queue (Solver * solver, Queue * q, Var * v) {
  assert (!v || q == queue (solver, v));
  q->search = v;
#ifndef NLOG
  if (v) {
    SOG ("updating to search %s variable %d next in %s queue",
      var_type (v), (int)(long)(v - solver->vars), queue_type (solver, q));
  } else SOG ("empty %s queue", queue_type (solver, q));
#endif
}

static void enqueue (Solver * solver, Var * v) {
  Queue * q = queue (solver, v);
  assert (!v->next);
  assert (!v->prev);
  v->stamp = ++solver->queue.stamp;
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
  Frame res;
  res.seen = res.flipped = 0;
  res.decision = decision;
  res.counted = -1;
  res.prev = 0;
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
  SOG ("initial reduce interval %ld", solver->limit.reduce.interval);
  SOG ("initial reduce increment %ld", solver->limit.reduce.increment);
  SOG ("initial reduce learned limit %ld",
    solver->limit.reduce.learned);
}

static void init_restart_limit (Solver * solver) {
  solver->limit.restart.conflicts = MAX (options.restartint, 1);
  SOG ("initial restart conflict limit %ld",
    solver->limit.restart.conflicts);
}

static void set_subsumed_limit (Solver * solver) {
  solver->limit.subsumed += solver->cnf.primal->irredundant/10;
  SOG ("subsumed limit %ld", solver->limit.subsumed);
}

static void init_limits (Solver * solver) {
  init_reduce_limit (solver);
  init_restart_limit (solver);
  set_subsumed_limit (solver);
  solver->limit.models.count = LONG_MAX;
  solver->limit.models.report = 1;
  solver->limit.models.log2report = 0;
  init_number_from_unsigned (solver->limit.count.report, 1);
  solver->limit.count.log2report = 0;
}

void limit_number_of_partial_models (Solver * solver, long limit) {
  assert (limit > 0);
  solver->limit.models.count = limit;
  msg (1, "number of partial models limited to %ld", limit);
}

static void enable_model_printing (Solver * solver, Name name) {
  solver->name = name;
  solver->model_printing_enabled = 1;
  msg (1, "enabled printing of partial models");
}

static void force_splitting_on_relevant_first (Solver * solver) {
  assert (!solver->split_on_relevant_first);
  msg (2, "forcing to split on relevant shared variables first");
  solver->split_on_relevant_first = 1;
}

Solver * new_solver (CNF * primal,
                     IntStack * shared, IntStack * relevant,
		     CNF * dual) {
  assert (!relevant);
  assert (primal);
  Solver * solver;
  NEW (solver);
  solver->cnf.primal = primal;
  if (dual) {
    SOG (
      "new solver over %ld primal and %ld dual clauses %ld shared variables",
      (long) COUNT (primal->clauses),
      (long) COUNT (dual->clauses),
      (long) COUNT (*shared));
    solver->cnf.dual = dual;
    solver->dual_solving_enabled = 1;
  } else {
    SOG (
      "new solver over %ld clauses and %ld shared variables",
      (long) COUNT (primal->clauses),
      (long) COUNT (*shared));
    solver->cnf.dual = 0;
    solver->dual_solving_enabled = 0;
  }
  solver->sorted = shared;
  int max_input_var = 0;
  for (const int * p = shared->start; p < shared->top; p++) {
    int input = abs (*p);
    assert (0 < input);
    if (input > max_input_var) max_input_var = input;
  }
  SOG ("maximum shared variable index %d", max_input_var);
  assert (COUNT (*shared) == max_input_var);
  int max_var = max_input_var;
  int max_primal_var = maximum_variable_index (primal);
  SOG ("maximum variable index %d in primal CNF", max_primal_var);
  if (max_var <  max_primal_var) {
    SOG ("%d primal gate variables %d ... %d",
      max_primal_var - max_var, max_var + 1, max_primal_var);
    max_var = max_primal_var;
  } else SOG ("no primal gate variables");
  if (dual) {
    int max_dual_var = dual ? maximum_variable_index (dual) : 0;
    SOG ("maximum variable index %d in dual CNF", max_dual_var);
    if (max_var <  max_dual_var) {
      SOG ("%d dual gate variables %d ... %d",
        max_dual_var - max_var, max_var + 1, max_dual_var);
      max_var = max_dual_var;
    } else SOG ("no dual gate variables");
    int min_non_input_var_in_dual =
      minimum_variable_index_above (dual, max_input_var);
    SOG ("minimum variable index %d above %d in dual CNF",
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
  solver->max_lit = 2*max_var + 1;
  solver->max_input_var = max_input_var;
  SOG ("maximum variable index %d", solver->max_var);
  solver->max_primal_or_input_var = MAX (max_primal_var, max_input_var);
  SOG ("%d primal gate or input variables",
    solver->max_primal_or_input_var);
  solver->unassigned_primal_and_input_variables =
    solver->max_primal_or_input_var;
  solver->unassigned_input_variables = max_input_var;
  ALLOC (solver->vars, solver->max_var + 1);
  ALLOC (solver->occs.primal, solver->max_lit + 1);
  if (dual)
    ALLOC (solver->occs.dual, solver->max_lit + 1);
  for (int idx = 1; idx <= solver->max_var; idx++)
    solver->vars[idx].phase = solver->phase;
  assert (max_primal_var <= max_var);
  if (relevant) {
    for (const int * p = relevant->start; p < relevant->top; p++) {
      const int idx = * p;
      assert (0 < idx);
      assert (idx <= max_input_var);
      Var * v = var (solver, idx);
      assert (!v->type);
      v->type = RELEVANT_VARIABLE;
    }
  }
  long count_relevant = 0, count_irrelevant = 0;
  for (int idx = 1; idx <= max_input_var; idx++) {
    Var * v = var (solver, idx);
    if (!relevant || v->type) {
      assert (!relevant || v->type == RELEVANT_VARIABLE);
      SOG ("relevant shared variable %d", idx);
      count_relevant++;
    } else {
      SOG ("irrelevant shared variable %d", idx);
      v->type = IRRELEVANT_VARIABLE;
      count_irrelevant++;
    }
  }
  SOG ("found %ld relevant shared variables", count_relevant);
  SOG ("found %ld irrelevant shared variables", count_irrelevant);
  assert (count_relevant + count_irrelevant == COUNT (*shared));
  assert (!relevant || count_relevant == COUNT (*relevant));
  long count_primal = 0;
  for (int idx = max_input_var + 1; idx <= max_primal_var; idx++) {
    Var * v = var (solver, idx);
    assert (!v->type);
    SOG ("primal variable %d", idx);
    v->type = PRIMAL_VARIABLE;
    count_primal++;
  }
  SOG ("found %ld private primal variables", count_primal);
  if (dual) {
    long count_dual = 0;
    for (int idx = solver->max_primal_or_input_var + 1;
         idx <= max_var;
	 idx++) {
      Var * v = var (solver, idx);
      assert (!v->type);
      SOG ("dual variable %d", idx);
      v->type = DUAL_VARIABLE;
      count_dual++;
    }
    SOG ("found %ld private dual variables", count_dual);
  }
  SOG ("connecting variables");
  for (int idx = 1; idx <= solver->max_primal_or_input_var; idx++)
    enqueue (solver, var (solver, idx));
  solver->phase = options.phaseinit ? 1 : -1;
  SOG ("default initial phase %d", solver->phase);
  init_limits (solver);
  PUSH (solver->frames, new_frame (solver, 0));
  assert (COUNT (solver->frames) == solver->level + 1);
  init_number (solver->count);
  if (options.relevant) force_splitting_on_relevant_first (solver);
  return solver;
}

static int is_primal_var (Var * v) { return v->type == PRIMAL_VARIABLE; }
static int is_relevant_var (Var * v) { return v->type == RELEVANT_VARIABLE; }
static int is_irrelevant_var (Var * v) { return v->type == IRRELEVANT_VARIABLE; }
static int is_dual_var (Var * v) { return v->type == DUAL_VARIABLE; }

static int is_input_var (Var * v) {
  return is_relevant_var (v) || is_irrelevant_var (v);
}

static int is_primal_or_input_var (Var * v) {
  return v->type != DUAL_VARIABLE;
}

static int is_dual_or_input_var (Var * v) {
  return v->type != PRIMAL_VARIABLE;
}

static Clauses * primal_occs (Solver * solver, int lit) {
  assert (is_primal_or_input_var (var (solver, lit)));
  const int pos = 2*abs (lit) + (lit > 0);
  assert (2 <= pos), assert (pos <= solver->max_lit);
  return solver->occs.primal + pos;
}

static Clauses * dual_occs (Solver * solver, int lit) {
  assert (is_dual_or_input_var (var (solver, lit)));
  const int pos = 2*abs (lit) + (lit > 0);
  assert (2 <= pos), assert (pos <= solver->max_lit);
  return solver->occs.dual + pos;
}

void delete_solver (Solver * solver) {
  SOG ("deleting solver");
  for (int idx = 1; idx <= solver->max_var; idx++) {
    Var * v = var (solver, idx);
    for (int sign = -1; sign <= 1; sign +=2) {
      const int lit = sign * idx;
      if (is_primal_or_input_var (v)) {
	Clauses * p = primal_occs (solver, lit);
	RELEASE (*p);
      }
      if (solver->dual_solving_enabled && is_dual_or_input_var (v)) {
	Clauses * d = dual_occs (solver, lit);
	RELEASE (*d);
      }
    }
  }
  DEALLOC (solver->occs.primal, solver->max_lit + 1);
  if (solver->dual_solving_enabled)
    DEALLOC (solver->occs.dual, solver->max_lit + 1);
  RELEASE (solver->frames);
  RELEASE (solver->trail);
  RELEASE (solver->seen);
  RELEASE (solver->clause);
  RELEASE (solver->levels);
  DEALLOC (solver->vars, solver->max_var+1);
  clear_number (solver->count);
  clear_number (solver->limit.count.report);
  DELETE (solver);
}

static CNF * cnf (Solver * solver, Clause * c) {
  return c->dual ? solver->cnf.dual : solver->cnf.primal;
}

static int lit_sign (int literal) { return literal < 0 ? -1 : 1; }


static void dec_unassigned (Solver * solver, Var * v) {
  if (is_primal_or_input_var (v)) {
    assert (solver->unassigned_primal_and_input_variables > 0);
    solver->unassigned_primal_and_input_variables--;
  }
  if (is_input_var (v)) {
    assert (solver->unassigned_input_variables > 0);
    solver->unassigned_input_variables--;
  }
}

static void inc_unassigned (Solver * solver, Var * v) {
  if (is_primal_or_input_var (v)) {
    solver->unassigned_primal_and_input_variables++;
    assert (solver->unassigned_primal_and_input_variables > 0);
  }
  if (is_input_var (v)) {
    solver->unassigned_input_variables++;
    assert (solver->unassigned_input_variables > 0);
  }
}

static void assign_temporarily (Solver * solver, int lit) {
  int idx = abs (lit);
  assert (0 < idx), assert (idx <= solver->max_var);
  Var * v = solver->vars + idx;
  assert (!v->val);
  v->val = lit_sign (lit);
  SOG ("assign %d temporarily", lit);
  dec_unassigned (solver, v);
}

static void unassign_temporarily (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  assert (v->val == lit_sign (lit));
  v->val = 0;
  SOG ("unassign %d temporarily", lit);
  inc_unassigned (solver, v);
}

static void assign (Solver * solver, int lit, Clause * reason) {
  int idx = abs (lit);
  assert (0 < idx), assert (idx <= solver->max_var);
  Var * v = solver->vars + idx;
  if (!reason) {
    assert (v->decision == DECISION || v->decision == FLIPPED);
    SOG ("%s assign %d %s", var_type (v), lit,
      v->decision == FLIPPED ? "flipped" : "decision"); 
  } else SOGCLS (reason, "%s assign %d reason", var_type (v), lit);
  assert (!v->val);
  if (lit < 0) v->val = v->phase = -1;
  else v->val = v->phase = 1;
  v->level = solver->level;
  if (v->level) {
    v->reason = reason;
    if (reason)
      mark_clause_active (v->reason, cnf (solver, v->reason));
  } else {
    if (is_dual_var (v)) solver->dual_non_input_fixed++;
    else solver->primal_or_input_fixed++;
    v->reason = 0;
  }
  PUSH (solver->trail, lit);
  dec_unassigned (solver, v);
}

static DecisionType decision_type (Solver * solver, int lit) {
  return var (solver, lit)->decision;
}

static Frame * last_frame (Solver * solver) {
  assert (0 <= solver->level);
  assert (solver->level < (int) COUNT (solver->frames));
  return solver->frames.start + solver->level;
}

/*------------------------------------------------------------------------*/

static void adjust_next (Solver * solver, int next) {
  assert (0 <= next), assert (next <= COUNT (solver->trail));
  if (next < solver->next.primal) solver->next.primal = next;
  if (next < solver->next.dual) solver->next.dual = next;
}

/*------------------------------------------------------------------------*/

// The number of levels in the solver consists of both decision and flipped
// levels.  After a variable is flipped its solver level remains the same,
// but both its decision type and its frame become flipped.

static void inc_solver_level (Solver * solver) {
  solver->level++;
  SOG ("incremented solver level");
}

/*------------------------------------------------------------------------*/

// Maintain a global count 'num_decision_levels' of actual decision levels.

static void inc_decision_levels (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (!f->flipped);
  assert (decision_type (solver, f->decision) == DECISION);
  solver->num_decision_levels++;
  f->prev = solver->last_decision_level;
  solver->last_decision_level = solver->level;
  SOG ("incremented number decision levels to %d",
    solver->num_decision_levels);
  SOG ("new last decision level %d", solver->last_decision_level);
}

static void dec_decision_levels (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (!f->flipped);
  assert (solver->num_decision_levels > 0);
  solver->num_decision_levels--;
  solver->last_decision_level = f->prev;
  SOG ("decremented number decision levels to %d",
    solver->num_decision_levels);
  SOG ("new last decision level %d", solver->last_decision_level);
}

/*------------------------------------------------------------------------*/

// Maintain a global count 'num_flipped_levels' of actual flipped levels.

static void inc_flipped_levels (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (f->flipped);
  assert (decision_type (solver, f->decision) == FLIPPED);
  solver->num_flipped_levels++;
  f->prev = solver->last_flipped_level;
  solver->last_flipped_level = solver->level;
  SOG ("incremented number flipped levels to %d",
    solver->num_flipped_levels);
  SOG ("new last flipped level %d", solver->last_flipped_level);
}

static void dec_flipped_levels (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (f->flipped);
  assert (solver->num_flipped_levels > 0);
  solver->num_flipped_levels--;
  solver->last_flipped_level = f->prev;
  SOG ("decremented number flipped levels to %d",
    solver->num_flipped_levels);
  SOG ("new last flipped level %d", solver->last_flipped_level);
}

/*------------------------------------------------------------------------*/

static void assume_decision (Solver * solver, int lit) {
  inc_solver_level (solver);
  SOG ("assume decision %d", lit);
  stats.decisions++;
  PUSH (solver->frames, new_frame (solver, lit));
  Var * v = var (solver, lit);
  assert (!v->decision);
  assert (v->decision == UNDECIDED);
  v->decision = DECISION;
  inc_decision_levels (solver);
  assign (solver, lit, 0);
}

static void dec_level (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  if (f->flipped) dec_flipped_levels (solver);
  else dec_decision_levels (solver);
  solver->level--;
  SOG ("decremented solver level");
  POP (solver->frames);
  assert (COUNT (solver->frames) == solver->level + 1);
}

static void flip_last_decision (Solver * solver) {
  stats.flipped++;
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  int decision = f->decision;
  assert (TOP (solver->trail) == decision);
  assert (val (solver, decision) > 0);
  Var * v = var (solver, decision);
  assert (v->level == solver->level);
  assert (v->decision == DECISION);
  assert (!f->flipped);
  dec_decision_levels (solver);
  v->decision = FLIPPED;
  f->flipped = 1;
  SOG ("%s flip %d", var_type (v), decision);
  inc_flipped_levels (solver);
  SOG ("%s assign %d", var_type (v), -decision);
  int n = COUNT (solver->trail) - 1;
  assert (PEEK (solver->trail, n) == decision);
  POKE (solver->trail, n, -decision);
  adjust_next (solver, n);
  v->val = -v->val;
}

/*------------------------------------------------------------------------*/

static DecisionType unassign (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  assert (solver->level == v->level);
  SOG ("%s unassign %d", var_type (v), lit);
  assert (v->val);
  v->val = 0;
  const DecisionType res = v->decision;
  v->decision = UNDECIDED;
  if (v->reason)
    mark_clause_inactive (v->reason, cnf (solver, v->reason));
  if (!is_dual_var (v)) {
    Queue * q = queue (solver, v);
    if (!q->search || q->search->stamp < v->stamp)
      update_queue (solver, q, v);
  }
  inc_unassigned (solver, v);
  return res;
}

static void connect_primal_literal (Solver * solver, Clause * c, int lit) {
  assert (!c->dual);
  SOGCLS (c, "connecting literal %d to", lit);
  Clauses * cs = primal_occs (solver, lit);
  PUSH (*cs, c);
}

static void connect_primal_clause (Solver * solver, Clause * c) {
  assert (!c->dual);
  assert (c->size > 1);
  connect_primal_literal (solver, c, c->literals[0]);
  connect_primal_literal (solver, c, c->literals[1]);
}

static int connect_primal_cnf (Solver * solver) {
  assert (!solver->level);
  CNF * cnf = solver->cnf.primal;
  Clauses * clauses = &cnf->clauses;
  SOG ("connecting %ld primal clauses to solver",
    (long) COUNT (*clauses));
  for (Clause ** p = clauses->start; p < clauses->top; p++) {
    Clause * c = *p;
    assert (c->dual == cnf->dual);
    if (c->size == 0) {
      SOG ("found empty clause in primal CNF");
      return 0;
    } else if (c->size == 1) {
      int unit = c->literals[0];
      SOG ("found unit clause %d in primal CNF", unit);
      int tmp = val (solver, unit);
      if (tmp > 0) {
	SOG ("ignoring already satisfied unit %d", unit);
	continue;
      }
      if (tmp < 0) {
	SOG ("found already falsified unit %d in primal CNF", unit);
	return 0;
      }
      assign (solver, unit, c);
    } else {
      assert (c->size > 1);
      connect_primal_clause (solver, c);
    }
  }
  return 1;
}

static void connect_dual_literal (Solver * solver, Clause * c, int lit) {
  assert (c->dual);
  SOGCLS (c, "connecting literal %d to", lit);
  Clauses * cs = dual_occs (solver, lit);
  PUSH (*cs, c);
}

static void connect_dual_clause (Solver * solver, Clause * c) {
  assert (c->dual);
  assert (c->size > 1);
  connect_dual_literal (solver, c, c->literals[0]);
  connect_dual_literal (solver, c, c->literals[1]);
}

static void print_model (Solver * solver) {
  assert (solver->model_printing_enabled);
  const int n = COUNT (*solver->sorted);
  int first = 1;
  for (int i = 0; i < n; i++) {
    const int lit = PEEK (*solver->sorted, i);
    const int tmp = val (solver, lit);
    if (!tmp) continue;
    if (!first) fputc (' ', stdout);
    if (tmp < 0) fputc ('!', stdout);
    fputs (solver->name.get (solver->name.state, lit), stdout);
    first = 0;
  }
  fputc ('\n', stdout);
}

static int model_limit_reached (Solver * solver) {
  const long limit = solver->limit.models.count;
  const int res = (stats.models.counted >= limit);
  if (res) msg (1, "reached partial models limit %ld", limit);
  return res;
}

#if 0
static int signed_marked_literal (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  int res = v->seen;
  if (lit < 0) res = -res;
  return res;
}
#endif

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

static int remaining_variables (Solver * solver) {
  return solver->max_primal_or_input_var - solver->primal_or_input_fixed;
}

static void primal_header (Solver * solver) {
  assert (!solver->dual_solving_enabled);
  msg (1, "");
  msg (1,
    "  "
    "        "
    "        "
    "          "
    "  learned"
    "   primal"
    "          "
    "  log2 "
    "  log2");
  msg (1,
    "  "
    "    time"
    "  memory"
    " conflicts"
    "  clauses"
    "  clauses"
    " variables"
    " models"
    " count");
  msg (1, "");
}

static void inc_model_report_limit (Solver * solver) {
  while (stats.models.counted >= solver->limit.models.report) {
    solver->limit.models.log2report++;
    solver->limit.models.report *= 2;
  }
  while (cmp_number (solver->count, solver->limit.count.report) >= 0) {
    solver->limit.count.log2report++;
    multiply_number_by_power_of_two (solver->limit.count.report, 1);
  }
}

static void
primal_report (Solver * solver, int verbosity, const char type) {
  assert (!solver->dual_solving_enabled);
  if (options.verbosity < verbosity) return;
  if (!(stats.reports++ % 20)) primal_header (solver);
  inc_model_report_limit (solver);
  msg (1,
    "%c"
    " %8.2f"
    " %7.1f"
    " %9ld"
    " %8ld"
    " %8ld"
    " %8ld"
    " %6ld"
    " %6ld",
    type,
    process_time (),
    current_resident_set_size ()/(double)(1<<20),
    stats.conflicts,
    solver->cnf.primal->redundant,
    solver->cnf.primal->irredundant,
    remaining_variables (solver),
    solver->limit.models.log2report-1,
    solver->limit.count.log2report-1);
}

static void dual_header (Solver * solver) {
  assert (solver->dual_solving_enabled);
  msg (1, "");
  msg (1,
    "  "
    "        "
    "        "
    "          "
    "  learned"
    "   primal"
    "   dual  "
    "variables"
    "  log2"
    "   log2");
  msg (1,
    "  "
    "    time"
    "  memory"
    " conflicts"
    "  clauses"
    "  clauses"
    "  clauses"
    "        "
    " models"
    "  count");
  msg (1, "");
}

static void
dual_report (Solver * solver, int verbosity, const char type) {
  assert (solver->dual_solving_enabled);
  if (options.verbosity < verbosity) return;
  if (!(stats.reports++ % 20)) dual_header (solver);
  assert (solver->cnf.dual);
  assert (!solver->cnf.dual->redundant);
  inc_model_report_limit (solver);
  msg (1,
    "%c"
    " %8.2f"
    " %7.1f"
    " %9ld"
    " %8ld"
    " %8ld"
    " %8ld"
    " %7ld"
    " %6ld"
    " %6ld",
    type,
    process_time (),
    current_resident_set_size ()/(double)(1<<20),
    stats.conflicts,
    solver->cnf.primal->redundant,
    solver->cnf.primal->irredundant,
    solver->cnf.dual->irredundant,
    remaining_variables (solver),
    solver->limit.models.log2report-1,
    solver->limit.count.log2report-1);
}

static void report (Solver * solver, int verbosity, const char type) {
  if (solver->dual_solving_enabled) dual_report (solver, verbosity, type);
  else primal_report (solver, verbosity, type);
}

static void first_model (Solver * solver) {
  for (int idx = 1; idx <= solver->max_var; idx++) {
    Var * v = var (solver, idx);
    v->first = v->val;
  }
  if (solver->dual_solving_enabled && !solver->split_on_relevant_first)
    force_splitting_on_relevant_first (solver);
}

static int new_model (Solver * solver) {
  int unassigned = solver->unassigned_input_variables;
  stats.models.counted++;
  SOG ("model %ld with %d unassigned shared variables",
    stats.models, unassigned);
  if (stats.models.counted == 1) first_model (solver);
  if (solver->model_printing_enabled) print_model (solver);
  add_power_of_two_to_number (solver->count, unassigned);
  if (stats.models.counted == solver->limit.models.report)
    report (solver, 1, '+');
  else report (solver, 3, 'm');
  return unassigned;
}

static int last_model (Solver * solver, int * unassigned_ptr) {
  assert (!model_limit_reached (solver));
  *unassigned_ptr = new_model (solver);
  return model_limit_reached (solver);
}

static int connect_dual_cnf (Solver * solver) {
  assert (!solver->level);
  if (!solver->dual_solving_enabled) return 1;
  CNF * cnf = solver->cnf.dual;
  Clauses * clauses = &cnf->clauses;
  SOG ("connecting %ld dual clauses to solver for counting",
    (long) COUNT (*clauses));
  for (Clause ** p = clauses->start; p < clauses->top; p++) {
    Clause * c = *p;
    assert (c->dual == cnf->dual);
    if (c->size == 0) {
      SOG ("found empty clause in dual CNF");
      return 0;
    } else if (c->size == 1) {
      int unit = c->literals[0];
      SOG ("found unit clause %d in dual CNF", unit);
      int tmp = val (solver, unit);
      if (tmp > 0) {
	SOG ("ignoring already satisfied unit %d", unit);
	continue;
      }
      if (tmp < 0) {
	SOG ("found already falsified unit %d in dual CNF", unit);
        new_model (solver);
	return 0;
      }
      if (is_input_var (var (solver, unit))) {
	assign_temporarily (solver, -unit);
	int unassigned;
	if (last_model (solver, &unassigned)) return 0;
	unassign_temporarily (solver, -unit);
	// TODO: nothing to do with 'unassigned'?
      }
      assign (solver, unit, c);
    } else {
      assert (c->size > 1);
      connect_dual_clause (solver, c);
    }
  }
  return 1;
}

static void report_iterating (Solver * solver) {
  if (!solver->found_new_fixed_variable) return;
  solver->found_new_fixed_variable = 0;
  report (solver, 1, 'i');
}

static Clause * primal_propagate (Solver * solver) {
  Clause * res = 0;
  while (!res && solver->next.primal < COUNT (solver->trail)) {
    int lit = solver->trail.start[solver->next.primal++];
    if (is_dual_var (var (solver, lit))) continue;
    assert (val (solver, lit) > 0);
    SOG ("primal propagating %d", lit);
    stats.propagated++;
    Clauses * o = primal_occs (solver, -lit);
    Clause ** q = o->start, ** p = q;
    while (!res && p < o->top) {
      Clause * c = *q++ = *p++;
      if (c->garbage) { q--; continue; }
      assert (!c->dual);
      SOGCLS (c, "visiting while propagating %d", lit);
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
	connect_primal_literal (solver, c, replacement);
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
  report_iterating (solver);
  return res;
}

static Frame * current_frame (Solver * solver) {
  assert (solver->level < COUNT (solver->frames));
  return solver->frames.start + solver->level;
}

static void save_count (Solver * solver, int counted) {
  Frame * frame = current_frame (solver);
  assert (frame->counted < 0);
  if (frame->flipped) {
    SOG ("saving count 2^%d", counted);
    frame->counted = counted;
  } else {
    SOG ("not saving count 2^%d since not flipped", counted);
  }
}

static Clause * dual_propagate (Solver * solver) {
  if (!solver->dual_solving_enabled) return 0;
  assert (solver->next.primal == COUNT (solver->trail));
  Clause * res = 0;
  while (!res && solver->next.dual < COUNT (solver->trail)) {
    int lit = solver->trail.start[solver->next.dual++];
    if (is_primal_var (var (solver, lit))) continue;
    assert (val (solver, lit) > 0);
    SOG ("dual propagating %d", lit);
    stats.propagated++;
    Clauses * o = dual_occs (solver, -lit);
    Clause ** q = o->start, ** p = q;
    while (!res && p < o->top) {
      Clause * c = *q++ = *p++;
      if (c->garbage) { q--; continue; }
      assert (c->dual);
      SOGCLS (c, "visiting while propagating %d", lit);
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
	connect_dual_literal (solver, c, replacement);
	q--;
      } else if (other_val) {
	assert (other_val < 0);
	SOGCLS (c, "conflict");
	res = c;
      } else if (is_input_var (var (solver, other))) {
	SOGCLS (c, "shared unit %d", other);
	assume_decision (solver, -other);
	SOGCLS (c, "conflict");
	res = c;
      } else {
	assert (is_dual_var (var (solver, other)));
	SOGCLS (c, "dual unit %d", other);
	assign (solver, other, c);
      }
    }
    if (res) {
      if (p < o->top) {
	while (p < o->top) *q++ = *p++;
	assert (solver->next.dual > 0);
	solver->next.dual--;
      }
    }
    o->top = q;
  }
  report_iterating (solver);
  return res;
}

static Var * next_queue (Solver * solver, Queue * queue) {
  Var * res = queue->search;
  while (res && res->val) 
    res = res->prev, stats.searched++;
  update_queue (solver, queue, res);
  return res;
}

static Var * next_decision (Solver * solver) {
  Var * relevant = next_queue (solver, &solver->queue.relevant);
  Var * res = 0;
  if (relevant) res = relevant;
  if (!res || !solver->split_on_relevant_first) {
    Var * irrelevant = next_queue (solver, &solver->queue.irrelevant);
    Var * primal = next_queue (solver, &solver->queue.primal);
    if (irrelevant && (!res || res->stamp < irrelevant->stamp))
      res = irrelevant;
    if (primal && (!res || res->stamp < primal->stamp))
      res = primal;
    if (!res) res = next_queue (solver, &solver->queue.dual);
  }
  assert (res);
  SOG ("next %s decision %d stamped %ld",
    var_type (res), var2idx (solver, res), res->stamp);
  return res;
}

static void decide (Solver * solver) {
  Var * v = next_decision (solver);
  int lit = v - solver->vars;
  if (v->phase < 0) lit = -lit;
  SOG ("%s decide %d", var_type (v), lit);
  assume_decision (solver, lit);
}

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

static void flush_primal_garbage_occurrences (Solver * solver) {
  long flushed = 0;
  for (int idx = 1; idx <= solver->max_primal_or_input_var; idx++)
    for (int sign = -1; sign <= 1; sign += 2) {
      int lit = sign * idx;
      Clauses * o = primal_occs (solver, lit);
      Clause ** q = o->start, ** p = q;
      while (p < o->top) {
	Clause * c = *p++;
	if (c->garbage) flushed++;
	else *q++ = c;
      }
      o->top = q;
      if (EMPTY (*o)) RELEASE (*o);
    }
  SOG ("flushed %ld primal garbage occurrences", flushed);
}

static void subsume (Solver * solver, Clause * c) {
  assert (!c->redundant);
  int limit = MAX (options.subsumelimit, 0);
  SOG ("trying to subsume at most %d clauses", limit);
  assert (!c->dual);
  mark_clause (solver, c);
  Clause ** p = solver->cnf.primal->clauses.top;
  int count = 0;
  while (p != solver->cnf.primal->clauses.start) {
    Clause * d = *--p;
    if (d == c) continue;
    if (d->garbage) break;
    if (subsumed (solver, d, c->size)) {
      SOGCLS (d, "subsumed");
      d->garbage = 1;
      stats.subsumed++;
      count++;
    } else if (!limit--) break;
  }
  unmark_clause (solver, c);
  SOG ("subsumed %d clauses", count);
  if (stats.subsumed <= solver->limit.subsumed) return;
  flush_primal_garbage_occurrences (solver);
  collect_garbage_clauses (solver->cnf.primal);
  set_subsumed_limit (solver);
  report (solver, 1, 's');
}

static void adjust_next_to_trail (Solver * solver) {
  adjust_next (solver, COUNT (solver->trail));
}

static void add_decision_blocking_clause (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  int first = f->decision;
  stats.blocked.clauses++;
  SOG ("adding decision blocking clause %ld for decision %d at level %d",
    stats.blocked.clauses, first, level (solver, first));
  assert (!f->flipped);
  assert (EMPTY (solver->clause));
  for (int level = solver->level; level > 0; level--) {
    f = solver->frames.start + level;
    if (f->flipped) continue;
    int decision = f->decision;
    PUSH (solver->clause, -decision);
    SOG ("adding %s %s literal %d from decision level %d",
      f->flipped ? "flipped" : "decision",
      var_type (var (solver, decision)), -decision, level);
  }
  const int size = COUNT (solver->clause);
  stats.blocked.literals += size;
  SOG ("found blocking clause of length %d", size);
  assert (size <= 1 || solver->clause.start[0] == -first);
  int other;
  for (;;) {
    other = TOP (solver->trail);
    const DecisionType decision = decision_type (solver, other);
    (void) POP (solver->trail);
    if (unassign (solver, other) != UNDECIDED) dec_level (solver);
    if (other == decision) break;
  }
  Clause * c = new_clause (solver->clause.start, size);
  assert (!c->glue), assert (!c->redundant);
  SOGCLS (c, "decision blocking");
  add_clause_to_cnf (c, solver->cnf.primal);
  if (size > 1) connect_primal_clause (solver, c);
  CLEAR (solver->clause);
  adjust_next_to_trail (solver);
  assign (solver, -first, c);
  if (!solver->level) solver->found_new_fixed_variable = 1;
  if (options.subsume) subsume (solver, c);
}

static int blocking (Solver * solver) {
  if (!options.block) return 0;
  assert (solver->level >= solver->num_flipped_levels);
  const int size = solver->num_decision_levels;
  SOG ("expecting blocking clause of size %d", size);
  assert (size == solver->level - solver->num_flipped_levels);
  return size <= options.blocklimit;
}

/*------------------------------------------------------------------------*/

// Without the dual part we find models as in a standard CDCL solvers, i.e.,
// if all variables are propagated in the primal part without getting a
// conflict then the formula is supposed to be satisfied.

static int satisfied (Solver * solver) {
  assert (solver->next.primal == COUNT (solver->trail));
  return !solver->unassigned_primal_and_input_variables;
}

// Common backtracking.

static void
backtrack_to_last_non_flipped_decision (Solver * solver, int counted) {
  assert (solver->last_decision_level > 0);
  stats.back.tracked++;
  SOG ("backtracking %ld to last non-flipped decision level %d",
    stats.back.tracked, solver->last_decision_level);
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    assert (v->level >= solver->last_decision_level);
    if (v->level == solver->last_decision_level) break;
    if (unassign (solver, lit) == FLIPPED) dec_level (solver);
    (void) POP (solver->trail);
  }
  if (blocking (solver)) {
    add_decision_blocking_clause (solver);
  } else {
    flip_last_decision (solver);
    if (counted >= 0) save_count (solver, counted);
    else assert (last_frame (solver)->counted < 0);
    adjust_next_to_trail (solver);
  }
}

static int backtrack_satisfied (Solver * solver) {
  SOG ("backtrack satisfied");
  assert (satisfied (solver));
  int counted;
  if (last_model (solver, &counted)) return 0;
  if (!solver->last_decision_level) {
    SOG ("no more decisions left");
    return 0;
  }
  backtrack_to_last_non_flipped_decision (solver, counted);
  return 1;
}

static void backtrack_on_primal_conflict (Solver * solver) {
  SOG ("backtracking due to primal conflict");
  backtrack_to_last_non_flipped_decision (solver, -1);
}

/*------------------------------------------------------------------------*/

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

static int jump_level (Solver * solver) {
  sort_clause (solver);
  const size_t size = COUNT (solver->clause);
  int res;
  if (size >= 2) {
    const int * lits = solver->clause.start;
    assert (var (solver, lits[0])->level == solver->level);
    assert (var (solver, lits[1])->level < solver->level);
    res = var (solver, lits[1])->level;
  } else res = 0;
  SOG ("jump level %d of learned clause of size %d", res, size);
  return res;
}

static Clause * learn_clause (Solver * solver, int glue, int level) {
  stats.learned++;
  const int size = COUNT (solver->clause);
  SOG ("learning clause number %ld of size %zd", stats.learned, size);
  Clause * res = new_clause (solver->clause.start, size);
  res->glue = glue;
  res->redundant = 1;
  res->recent = 1;
  SOGCLS (res, "learned new");
  add_clause_to_cnf (res, solver->cnf.primal);
  if (size > 1) connect_primal_clause (solver, res);
  CLEAR (solver->clause);
  return res;
}

static void discount (Solver * solver) {
  assert (options.discount);
  stats.models.discounted++;
  Frame * f = solver->frames.start + solver->level;
  assert (f->flipped);
  SOG ("discounting 2^%d", f->counted);
  sub_power_of_two_from_number (solver->count, f->counted);
  report (solver, 3, 'd');
}

static int
backjump_on_primal_conflict (Solver * solver, Clause * c, int level) {
  assert (c->size > 0);
  const int forced = c->literals[0];
  stats.back.jumped++;
  SOG ("back jump %ld to level %d", stats.back.jumped, level);
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    if (v->level == level) break;
    (void) POP (solver->trail);
    const DecisionType decision = unassign (solver, lit);
    if (decision == UNDECIDED) continue;
    if (decision == FLIPPED) discount (solver);
    dec_level (solver);
  }
  assert (!val (solver, forced));
  assert (solver->level == level);
  adjust_next_to_trail (solver);
  assign (solver, forced, c);
  if (!level) solver->found_new_fixed_variable = 1;
  return 1;
}

/*------------------------------------------------------------------------*/

static int resolve_primal_conflict (Solver * solver, Clause * conflict) {
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

static int analyze_primal (Solver * solver, Clause * conflict) {
  SOG ("analyze primal");
  assert (conflict);
  assert (!conflict->dual);
  if (!solver->last_decision_level) {
    SOG ("primal conflict without any decisions");
    return 0;
  }
  int glue = resolve_primal_conflict (solver, conflict), level, learn;
  if (options.learn) {
    level = jump_level (solver);
    assert (solver->last_flipped_level <= solver->level);
    if (solver->last_flipped_level > level) {
      if (options.discount) {
	SOG ("expecting to discount at least flipped frame %d",
	  solver->last_flipped_level);
	stats.back.discounting++;
	SOG ("discounting backtrack %ld", stats.back.discounting);
	learn = 0;
      } else {
	SOG ("flipped frame %d forces backtracking without discounting",
	  solver->last_flipped_level);
	SOG ("forced backtrack %ld", stats.back.forced);
	stats.back.forced++;
	learn = 0;
      }
    }
  } else {
    SOG ("learning disabled");
    level = solver->level - 1;
    learn = 0;
  }
  if (learn) {
    Clause * c = learn_clause (solver, glue, level);
    return backjump_on_primal_conflict (solver, c, level);
  } else {
    CLEAR (solver->clause);
    backtrack_on_primal_conflict (solver);
    return 1;
  }
}

/*------------------------------------------------------------------------*/

// Basic version without actual analysis part yet.

static int analyze_dual (Solver * solver, Clause * conflict) {
  SOG ("analyze dual");
  assert (conflict);
  assert (conflict->dual);
  int counted;
  if (last_model (solver, &counted)) return 0;
  if (!solver->last_decision_level) {
    SOG ("no more decisions left");
    return 0;
  }
  backtrack_to_last_non_flipped_decision (solver, counted);
  return 1;
}

/*------------------------------------------------------------------------*/

static int reducing (Solver * solver) {
  if (!options.reduce) return 0;
  if (!stats.conflicts &&
      solver->primal_or_input_fixed >
        solver->limit.reduce.primal_or_input_fixed) return 1;
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
  const int simplify =
    solver->primal_or_input_fixed >
      solver->limit.reduce.primal_or_input_fixed;
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
  solver->limit.reduce.primal_or_input_fixed =
    solver->primal_or_input_fixed;
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
  flush_primal_garbage_occurrences (solver);
  collect_garbage_clauses (primal);
  inc_reduce_limit (solver);
  report (solver, 1, '-');
}

/*------------------------------------------------------------------------*/

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
    assert (v->decision == DECISION);
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
    const DecisionType decision = unassign (solver, lit);
    if (decision != UNDECIDED) dec_level (solver);
    (void) POP (solver->trail);
  }
  adjust_next_to_trail (solver);
  report (solver, 2, 'r');
}

static void solve (Solver * solver) {
  if (model_limit_reached (solver)) return;
  if (!connect_primal_cnf (solver)) return;
  if (primal_propagate (solver)) return;
  if (satisfied (solver)) { (void) new_model (solver); return; }
  if (!connect_dual_cnf (solver)) return;
  for (;;) {
    Clause * conflict = primal_propagate (solver);
    if (conflict) {
      if (!analyze_primal (solver, conflict)) return;
    } else if (satisfied (solver)) {
      if (!backtrack_satisfied (solver)) return;
    } else {
      conflict = dual_propagate (solver);
      if (conflict) {
	if (!analyze_dual (solver, conflict)) return;
      } else if (reducing (solver)) reduce (solver);
      else if (restarting (solver)) restart (solver);
      else decide (solver);
    }
  }
}

/*------------------------------------------------------------------------*/

int primal_sat (Solver * solver) {
  msg (1, "primal checking");
  assert (!solver->dual_solving_enabled);
  limit_number_of_partial_models (solver, 1);
  solve (solver);
  return stats.models.counted ? 10 : 20;
}

int dual_sat (Solver * solver) {
  msg (1, "dual checking");
  assert (solver->dual_solving_enabled);
  limit_number_of_partial_models (solver, 1);
  solve (solver);
  return stats.models.counted ? 10 : 20;
}

void primal_count (Number models, Solver * solver) {
  msg (1, "primal counting");
  assert (!solver->dual_solving_enabled);
  solve (solver);
  copy_number (models, solver->count);
}

void dual_count (Number models, Solver * solver) {
  msg (1, "dual counting");
  assert (solver->dual_solving_enabled);
  solve (solver);
  copy_number (models, solver->count);
}

void primal_enumerate (Solver * solver, Name name) {
  msg (1, "primal enumeration");
  assert (!solver->dual_solving_enabled);
  enable_model_printing (solver, name);
  solve (solver);
}

void dual_enumerate (Solver * solver, Name name) {
  msg (1, "dual enumeration");
  assert (solver->dual_solving_enabled);
  enable_model_printing (solver, name);
  solve (solver);
}

/*------------------------------------------------------------------------*/

int deref (Solver * solver, int lit) {
  assert (stats.models.counted > 0);
  Var * v = var (solver, lit);
  int res = v->first;
  if (lit < 0) res = -res;
  return res;
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
    if (v->decision == UNDECIDED) fputc ('p', stdout);
    if (v->decision == DECISION) fputc ('d', stdout);
    if (v->decision == FLIPPED) fputc ('f', stdout);
  }
  fputc ('\n', stdout);
}
#endif
