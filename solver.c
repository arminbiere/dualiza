#include "headers.h"

/*------------------------------------------------------------------------*/

#ifdef NLOG
#define SOG(...) do { } while (0)
#define SOGCLS(...) do { } while (0)
#define SOGNUM(...) do { } while (0)
#else
#define SOG(FMT,ARGS...) LOG ("%d " FMT, solver->level, ##ARGS)
#define SOGCLS(C,FMT,ARGS...) LOGCLS (C, "%d " FMT, solver->level, ##ARGS)
#define SOGNUM(N,FMT,ARGS...) LOGNUM (N, "%d " FMT, solver->level, ##ARGS)
#endif

/*------------------------------------------------------------------------*/

#define RULE0(NAME) \
do { \
  rules.NAME++; \
  SOG ("RULE %s %ld", #NAME, rules.NAME); \
} while (0)

#define RULE1(NAME,LIT) \
do { \
  rules.NAME++; \
  SOG ("RULE %s %ld\t%d", #NAME, rules.NAME, LIT); \
} while (0)

#ifdef NLOG
#define RULE2(NAME,LIT,C) do { rules.NAME++; } while (0)
#else
#define RULE2(NAME,LIT,C) \
do { \
  rules.NAME++; \
  if (!options.logging) break; \
  printf ("c LOG %d RULE %s %ld\t%d", \
    solver->level, #NAME, rules.NAME, LIT); \
  for (int I = 0; I < C->size; I++) { \
    fputc (I ? ' ' : '\t', stdout); \
    printf ("%d", C->literals[I]); \
  } \
  fputc ('\n', stdout); \
  fflush (stdout); \
} while (0)
#endif

/*------------------------------------------------------------------------*/

typedef struct Var Var;
typedef enum Type Type;
typedef struct Queue Queue;
typedef struct Frame Frame;
typedef struct Limit Limit;
typedef enum Decision Decision;

typedef STACK (Var *) VarStack;
typedef STACK (Frame) FrameStack;

enum Type {
  UNCLASSIFIED_VARIABLE = 0,
  RELEVANT_VARIABLE = 1,
  IRRELEVANT_VARIABLE = 2,
  PRIMAL_VARIABLE = 3,
  DUAL_VARIABLE = 4,
};

enum Decision {
  UNDECIDED = 0,
  DECISION = 1,
  FLIPPED = 2,
};

struct Var {
  Type type;
  signed int val : 2;		// -1 = false, 0 = unassigned, 1 = true
  signed int phase : 2;		// saved value of previous assignment
  signed int first : 2;		// value in first model
  unsigned decision : 2;	// actually of type 'Decision'
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
  int trail;			// trail position of decision
  struct {
    int flipped;		// prev flipped level
    int decision;		// prev decision level
    int relevant;		// prev relevant level
  } prev;
  Number count;
  long counted;			// number of times 
};

struct Limit {
  struct {
    long learned, interval, increment;
    int primal_or_shared_fixed;
    int dual_or_shared_fixed;
  } reduce;
  struct {
    long conflicts;
    double slow, fast;
  } restart;
  long subsumed;
  struct { long count, report, log2report; } models;
  struct { Number report; long log2report; } count;
};

#define num_report_header_lines 3

struct Solver {

  Var * vars;

  char found_new_fixed_variable;
  char dual_solving_enabled;
  char model_printing_enabled;
  char split_on_relevant_first;
  char require_to_split_on_relevant_first_after_first_model;

  int max_var, max_lit;
  int max_shared_var, max_primal_or_shared_var;
  int level, phase;

  struct { int primal, dual; } next;

  int last_flipped_level, num_flipped_levels;
  int last_decision_level, num_decision_levels;
  int last_relevant_level, num_relevant_levels;

  int unassigned_primal_and_shared_variables;
  int unassigned_relevant_variables;
  int unassigned_shared_variables;

  int dual_or_shared_fixed;
  int primal_or_shared_fixed;

  Clauses units;

  IntStack clause;
  IntStack trail;
  IntStack levels;
  IntStack relevant;
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

  struct {
    int entries;
    CharStack buffer[num_report_header_lines];
    CharStack columns;
  } report;
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

#ifndef NLOG

static int level (Solver * solver, int lit) {
  assert (val (solver, lit));
  return var (solver, lit)->level;
}

#endif

static Queue * queue (Solver * solver, Var * v) {
  if (v->type == RELEVANT_VARIABLE) return &solver->queue.relevant;
  if (v->type == IRRELEVANT_VARIABLE) return &solver->queue.irrelevant;
  if (v->type == PRIMAL_VARIABLE) return &solver->queue.primal;
  assert (v->type == DUAL_VARIABLE);
  return &solver->queue.dual;
}

#ifndef NLOG

static const char * type (Var * v) {
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
      type (v), (int)(long)(v - solver->vars), queue_type (solver, q));
  } else SOG ("empty %s queue", queue_type (solver, q));
#endif
}

static void enqueue (Solver * solver, Var * v) {
  Queue * q = queue (solver, v);
  assert (!v->next);
  assert (!v->prev);
  v->stamp = ++solver->queue.stamp;
  SOG ("%s enqueue variable %d stamp %ld",
    type (v), var2idx (solver, v), v->stamp);
  if (!q->first) q->first = v;
  if (q->last) q->last->next = v;
  v->prev = q->last;
  q->last = v;
  if (!v->val) update_queue (solver, q, v);
}

static void dequeue (Solver * solver, Var * v) {
  Queue * q = queue (solver, v);
  SOG ("%s dequeue variable %d stamp %ld",
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

static void push_frame (Solver * solver, int decision) {
  Frame f;
  f.decision = decision;
  f.seen = f.flipped = 0;
  f.trail = COUNT (solver->trail);
  f.prev.flipped = f.prev.decision = f.prev.relevant = 0;
  f.counted = 0;
  init_number (f.count);
  PUSH (solver->frames, f);
}

static Frame * frame_at_level (Solver * solver, int level) {
  assert (0 <= level);
  assert ((size_t) level < COUNT (solver->frames));
  assert (level <= solver->level);
  return solver->frames.start + level;
}

static Frame * last_frame (Solver * solver) {
  return frame_at_level (solver, solver->level);
}

static int decision_at_level (Solver * solver, int level) {
  assert (level > 0);
  Frame * f = frame_at_level (solver, level);
  assert (!f->flipped);
  return f->decision;
}

static void pop_frame (Solver * solver) {
  Frame f = POP (solver->frames);
  clear_number (f.count);
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

static void set_subsumed_learned_limit (Solver * solver) {
  solver->limit.subsumed += solver->cnf.primal->irredundant/10;
  SOG ("new subsume learned clause limit %ld", solver->limit.subsumed);
}

static void init_limits (Solver * solver) {
  init_reduce_limit (solver);
  init_restart_limit (solver);
  set_subsumed_learned_limit (solver);
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

static void check_options_fixed () {
  assert (!options.block || !options.dual);
  assert (!options.block || !options.discount);
}

void fix_options () {
  if (options.block && options.dual) {
    msg (1, "dual propagation disables blocking clause learning");
    options.block = 0;
  } 
  if (options.block && options.discount) {
    msg (1, "discounting disables blocking clause learning");
    options.block = 0;
  } 
  check_options_fixed ();
}

Solver * new_solver (CNF * primal,
                     IntStack * shared, IntStack * relevant,
		     CNF * dual) {
  check_options_fixed ();
  assert (primal);
  Solver * solver;
  NEW (solver);
  solver->cnf.primal = primal;
  if (dual) {
    SOG ("new solver: %ld primal %ld, dual clauses, %ld shared variables",
      (long) COUNT (primal->clauses),
      (long) COUNT (dual->clauses),
      (long) COUNT (*shared));
    solver->cnf.dual = dual;
    solver->dual_solving_enabled = 1;
  } else {
    SOG ("new solver: %ld clauses, %ld shared variables",
      (long) COUNT (primal->clauses),
      (long) COUNT (*shared));
    solver->cnf.dual = 0;
    solver->dual_solving_enabled = 0;
  }
  int max_shared_var = 0;
  for (const int * p = shared->start; p < shared->top; p++) {
    int idx = abs (*p);
    assert (0 < shared);
    if (idx > max_shared_var) max_shared_var = idx;
  }
  SOG ("maximum shared variable index %d", max_shared_var);
  assert (COUNT (*shared) == max_shared_var);
  int max_var = max_shared_var;
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
    int min_non_shared_var_in_dual =
      minimum_variable_index_above (dual, max_shared_var);
    SOG ("minimum variable index %d above %d in dual CNF",
      min_non_shared_var_in_dual, max_shared_var);
#ifndef NDEBUG
    assert (min_non_shared_var_in_dual > max_primal_var);
    if (min_non_shared_var_in_dual < INT_MAX)
      assert (min_non_shared_var_in_dual <= max_dual_var);
#else
    (void) min_non_shared_var_in_dual;
#endif
  }
  solver->max_var = max_var;
  solver->max_lit = 2*max_var + 1;
  solver->max_shared_var = max_shared_var;
  SOG ("maximum variable index %d", solver->max_var);
  solver->max_primal_or_shared_var = MAX (max_primal_var, max_shared_var);
  SOG ("%d primal or shared variables",
    solver->max_primal_or_shared_var);
  solver->unassigned_primal_and_shared_variables =
    solver->max_primal_or_shared_var;
  solver->unassigned_shared_variables = max_shared_var;
  ALLOC (solver->vars, solver->max_var + 1);
  ALLOC (solver->occs.primal, solver->max_lit + 1);
  if (dual)
    ALLOC (solver->occs.dual, solver->max_lit + 1);
  for (int idx = 1; idx <= solver->max_var; idx++)
    solver->vars[idx].phase = solver->phase;
  assert (max_primal_var <= max_var);
  long count_relevant = 0, count_irrelevant = 0;

  // TODO remove this test and assume 'relevant' non zero instead.
  
  if (relevant) {
    for (const int * p = relevant->start; p < relevant->top; p++) {
      const int idx = * p;
      assert (0 < idx);
      assert (idx <= max_shared_var);
      Var * v = var (solver, idx);
      assert (!v->type);
      v->type = RELEVANT_VARIABLE;
    }
  }
  for (int idx = 1; idx <= max_shared_var; idx++) {
    Var * v = var (solver, idx);
    if (!relevant || v->type == RELEVANT_VARIABLE) {
      assert (!relevant || v->type == RELEVANT_VARIABLE);
      SOG ("relevant shared variable %d", idx);
      PUSH (solver->relevant, idx);
      v->type = RELEVANT_VARIABLE;
      count_relevant++;
    } else {
      SOG ("irrelevant shared variable %d", idx);
      v->type = IRRELEVANT_VARIABLE;
      count_irrelevant++;
    }
  }
  assert (count_relevant == COUNT (solver->relevant));
  assert (count_irrelevant == max_shared_var - count_relevant);
  SOG ("found %ld relevant shared variables", count_relevant);
  SOG ("found %ld irrelevant shared variables", count_irrelevant);
  assert (count_relevant + count_irrelevant == COUNT (*shared));
  assert (!relevant || count_relevant == COUNT (*relevant));
  solver->unassigned_relevant_variables = count_relevant;
  long count_primal = 0;
  for (int idx = max_shared_var + 1; idx <= max_primal_var; idx++) {
    Var * v = var (solver, idx);
    assert (!v->type);
    SOG ("primal variable %d", idx);
    v->type = PRIMAL_VARIABLE;
    count_primal++;
  }
  SOG ("found %ld private primal variables", count_primal);
  if (dual) {
    long count_dual = 0;
    for (int idx = solver->max_primal_or_shared_var + 1;
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
  for (int idx = 1; idx <= solver->max_primal_or_shared_var; idx++)
    enqueue (solver, var (solver, idx));
  solver->phase = options.phaseinit ? 1 : -1;
  SOG ("default initial phase %d", solver->phase);
  init_limits (solver);
  push_frame (solver, 0);
  assert (COUNT (solver->frames) == solver->level + 1);
  init_number (solver->count);
  if (options.relevant) {
    msg (1, "forced to split on relevant variables first");
    solver->split_on_relevant_first = 1;
  } else {
    assert (!solver->split_on_relevant_first);
    const char * reason;
         if (dual)             reason = "in dual mode";
    else if (count_irrelevant) reason = "with irrelevant variables";
    else if (count_primal)     reason = "with primal variables";
    else                       reason = 0;
    if (reason) {
      msg (1, "split on relevant variables after first model (%s)", reason);
      solver->require_to_split_on_relevant_first_after_first_model = 1;
    }
  }
  return solver;
}

static int is_primal_var (Var * v) {
  return v->type == PRIMAL_VARIABLE;
}

static int is_relevant_var (Var * v) {
  return v->type == RELEVANT_VARIABLE;
}

static int is_irrelevant_var (Var * v) {
  return v->type == IRRELEVANT_VARIABLE;
}

static int is_dual_var (Var * v) {
  return v->type == DUAL_VARIABLE;
}

static int is_shared_var (Var * v) {
  return is_relevant_var (v) || is_irrelevant_var (v);
}

static int is_primal_or_shared_var (Var * v) {
  return v->type != DUAL_VARIABLE;
}

static int is_dual_or_shared_var (Var * v) {
  return v->type != PRIMAL_VARIABLE;
}

static Clauses * primal_occs (Solver * solver, int lit) {
  assert (is_primal_or_shared_var (var (solver, lit)));
  const int pos = 2*abs (lit) + (lit > 0);
  assert (2 <= pos), assert (pos <= solver->max_lit);
  return solver->occs.primal + pos;
}

static Clauses * dual_occs (Solver * solver, int lit) {
  assert (is_dual_or_shared_var (var (solver, lit)));
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
      if (is_primal_or_shared_var (v)) {
	Clauses * p = primal_occs (solver, lit);
	RELEASE (*p);
      }
      if (solver->dual_solving_enabled && is_dual_or_shared_var (v)) {
	Clauses * d = dual_occs (solver, lit);
	RELEASE (*d);
      }
    }
  }
  DEALLOC (solver->occs.primal, solver->max_lit + 1);
  if (solver->dual_solving_enabled)
    DEALLOC (solver->occs.dual, solver->max_lit + 1);
  for (Frame * f = solver->frames.start; f != solver->frames.top; f++)
    clear_number (f->count);
  RELEASE (solver->frames);
  RELEASE (solver->trail);
  RELEASE (solver->seen);
  RELEASE (solver->relevant);
  RELEASE (solver->clause);
  RELEASE (solver->levels);
  RELEASE (solver->units);
  DEALLOC (solver->vars, solver->max_var+1);
  for (int i = 0; i < num_report_header_lines; i++)
    RELEASE (solver->report.buffer[i]);
  RELEASE (solver->report.columns);
  clear_number (solver->count);
  clear_number (solver->limit.count.report);
  DELETE (solver);
}

static CNF * cnf (Solver * solver, Clause * c) {
  return c->dual ? solver->cnf.dual : solver->cnf.primal;
}

static int lit_sign (int literal) { return literal < 0 ? -1 : 1; }

static void dec_unassigned (Solver * solver, Var * v) {
  if (is_primal_or_shared_var (v)) {
    assert (solver->unassigned_primal_and_shared_variables > 0);
    solver->unassigned_primal_and_shared_variables--;
  }
  if (is_shared_var (v)) {
    assert (solver->unassigned_shared_variables > 0);
    solver->unassigned_shared_variables--;
  }
  if (is_relevant_var (v)) {
    assert (solver->unassigned_relevant_variables > 0);
    solver->unassigned_relevant_variables--;
  }
}

static void inc_unassigned (Solver * solver, Var * v) {
  if (is_primal_or_shared_var (v)) {
    solver->unassigned_primal_and_shared_variables++;
    assert (solver->unassigned_primal_and_shared_variables > 0);
  }
  if (is_shared_var (v)) {
    solver->unassigned_shared_variables++;
    assert (solver->unassigned_shared_variables > 0);
  }
  if (is_relevant_var (v)) {
    solver->unassigned_relevant_variables++;
    assert (solver->unassigned_relevant_variables > 0);
  }
}

#if 0
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
#endif

static void assign (Solver * solver, int lit, Clause * reason) {
  int idx = abs (lit);
  assert (0 < idx), assert (idx <= solver->max_var);
  Var * v = solver->vars + idx;
  if (!reason) {
    assert (v->decision == DECISION || v->decision == FLIPPED);
    SOG ("%s assign %d %s", type (v), lit,
      v->decision == FLIPPED ? "flipped" : "decision"); 
  } else SOGCLS (reason, "%s assign %d reason", type (v), lit);
  assert (!v->val);
  if (lit < 0) v->val = v->phase = -1;
  else v->val = v->phase = 1;
  v->level = solver->level;
  if (v->level) {
    v->reason = reason;
    if (reason)
      mark_clause_active (v->reason, cnf (solver, v->reason));
  } else {
    if (is_primal_or_shared_var (v)) solver->primal_or_shared_fixed++;
    if (is_dual_or_shared_var (v)) solver->dual_or_shared_fixed++;
    v->reason = 0;
  }
  PUSH (solver->trail, lit);
  dec_unassigned (solver, v);
}

#ifndef NDEBUG

static Decision decision_type (Solver * solver, int lit) {
  return var (solver, lit)->decision;
}

#endif

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
// Since we also need precise information about the last decision level with
// a relevant variable as decision we have a separate linked list of
// relevant decision levels, which is also updated here.

static void inc_decision_levels (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (!f->flipped);
  assert (decision_type (solver, f->decision) == DECISION);
  solver->num_decision_levels++;
  f->prev.decision = solver->last_decision_level;
  solver->last_decision_level = solver->level;
  SOG ("incremented number decision levels to %d",
    solver->num_decision_levels);
  SOG ("new last decision level %d", solver->last_decision_level);
  if (is_relevant_var (var (solver, f->decision))) {
    solver->num_relevant_levels++;
    f->prev.relevant = solver->last_relevant_level;
    solver->last_relevant_level = solver->level;
    SOG ("incremented number relevant levels to %d",
      solver->num_relevant_levels);
    SOG ("new last relevant level %d", solver->last_relevant_level);
  }
}

static void dec_decision_levels (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (!f->flipped);
  assert (solver->num_decision_levels > 0);
  solver->num_decision_levels--;
  solver->last_decision_level = f->prev.decision;
  SOG ("decremented number decision levels to %d",
    solver->num_decision_levels);
  SOG ("new last decision level %d", solver->last_decision_level);
  if (is_relevant_var (var (solver, f->decision))) {
    assert (solver->num_relevant_levels > 0);
    solver->num_relevant_levels--;
    solver->last_relevant_level = f->prev.relevant;
    SOG ("decremented number relevant levels to %d",
      solver->num_relevant_levels);
    SOG ("new last relevant level %d", solver->last_decision_level);
  }
}

/*------------------------------------------------------------------------*/

// Maintain a global count 'num_flipped_levels' of actual flipped levels.

static void inc_flipped_levels (Solver * solver) {
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (f->flipped);
  assert (decision_type (solver, f->decision) == FLIPPED);
  solver->num_flipped_levels++;
  f->prev.flipped = solver->last_flipped_level;
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
  solver->last_flipped_level = f->prev.flipped;
  SOG ("decremented number flipped levels to %d",
    solver->num_flipped_levels);
  SOG ("new last flipped level %d", solver->last_flipped_level);
}

/*------------------------------------------------------------------------*/

static void assume_decision (Solver * solver, int lit) {
  inc_solver_level (solver);
  SOG ("assume decision %d", lit);
  stats.decisions++;
  push_frame (solver, lit);
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
  pop_frame (solver);
  assert (COUNT (solver->frames) == solver->level + 1);
}

static void flip_decision (Solver * solver, long * rule) {
  stats.flipped++;
  assert (solver->level > 0);
  Frame * f = last_frame (solver);
  assert (!f->flipped);
  int decision = f->decision;
  f->decision = -decision;
  assert (PEEK (solver->trail, f->trail) == decision);
  assert (val (solver, decision) > 0);
  Var * v = var (solver, decision);
  assert (v->level == solver->level);
  assert (v->decision == DECISION);
  dec_decision_levels (solver);
  v->decision = FLIPPED;
  f->flipped = 1;
  SOG ("%s flip %d", type (v), decision);
  inc_flipped_levels (solver);
  SOG ("%s assign %d", type (v), -decision);
       if (rule == &rules.BP1F) RULE1 (BP1F, -decision);
  else if (rule == &rules.BP0F) RULE1 (BP0F, -decision);
  else if (rule == &rules.BN0F) RULE1 (BN0F, -decision);
  else SOG ("WARNING UNMATCHED RULE in '%s'", __FUNCTION__);
  POKE (solver->trail, f->trail, -decision);
  adjust_next (solver, f->trail);
  v->val = -v->val;
}

/*------------------------------------------------------------------------*/

static Decision unassign (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  assert (solver->level == v->level);
  SOG ("%s unassign %d", type (v), lit);
  assert (v->val);
  v->val = 0;
  const Decision res = v->decision;
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
      RULE0 (EP0);
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
       RULE0 (EP0);
       return 0;
      }
      RULE1 (UP, unit);
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
  const int n = COUNT (solver->relevant);
  int first = 1;
  for (int i = 0; i < n; i++) {
    const int lit = PEEK (solver->relevant, i);
    const int tmp = val (solver, lit);
    if (!tmp) continue;
    if (!is_relevant_var (var (solver, lit))) continue;
    if (!first) fputc (' ', stdout);
    if (tmp < 0) fputc ('!', stdout);
    fputs (solver->name.get (solver->name.state, lit), stdout);
    first = 0;
  }
  fputc ('\n', stdout);
}

static void print_discount (Solver * solver, Frame * f) {
  assert (solver->model_printing_enabled);
  for (long i = 0; i < f->counted; i++)
    fputs ("<DISCOUNT>\n", stdout);
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
  return solver->max_primal_or_shared_var - solver->primal_or_shared_fixed;
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

#define HEADER solver->report.buffer
#define COLUMNS solver->report.columns
#define ENTRIES solver->report.entries

static void
column (Solver * solver, const char * name,
	int precision, int percent_suffix, int flush_right, double value)
{
  const size_t t = 80;
  char tmp[t], fmt[16];
  const char * suffix = percent_suffix ? "%%" : "";
  sprintf (fmt, "%%.%df%s", precision, suffix);
  snprintf (tmp, t, fmt, value);
  tmp[t-1] = 0;
  size_t m = strlen (tmp);
  if (m < 3) m = 3;
  sprintf (fmt, "%%%zd.%df%s", m - percent_suffix, precision, suffix);
  snprintf (tmp, t, fmt, value);
  tmp[t-3] = tmp[t-2] = '.', tmp[t-1] = 0;
  PUSH (COLUMNS, ' ');
  for (const char * p = tmp; *p; p++)
    PUSH (COLUMNS, *p);
  PUSH (COLUMNS, 0);
  (void) POP (COLUMNS);
  size_t n = strlen (name);
  int shift;
  if (n <= m) shift = 0;
  else if (!flush_right) shift = (n - m)/2;
  else shift = 1;
  PUSH (HEADER[ENTRIES], ' ');
  while (n + COUNT (HEADER[ENTRIES]) < COUNT (COLUMNS) + shift)
    PUSH (HEADER[ENTRIES], ' ');
  for (const char * p = name; *p; p++)
    PUSH (HEADER[ENTRIES], *p);
  PUSH (HEADER[ENTRIES], 0);
  (void) POP (HEADER[ENTRIES]);
  if (++ENTRIES == num_report_header_lines) ENTRIES = 0;
}

static void report (Solver * solver, int verbosity, char ch)
{
  if (verbosity > options.verbosity) return;
  ENTRIES = solver->dual_solving_enabled ? num_report_header_lines-1: 0;
  CLEAR (COLUMNS);
  for (int i = 0; i < num_report_header_lines; i++)
    CLEAR (HEADER[i]);
  double space = current_resident_set_size () / (double)(1<<20);
  column (solver, "time", 2, 0, 0, process_time ());
  column (solver, "space", 1, 0, 0, space);
  column (solver, "conflicts", 0, 0, 0, stats.conflicts.primal);
  column (solver, "redundant", 0, 0, 0, solver->cnf.primal->redundant);
  column (solver, "irredundant", 0, 0, 0, solver->cnf.primal->irredundant);
  if (solver->dual_solving_enabled)
  column (solver, "dual", 0, 0, 0, solver->cnf.dual->irredundant);
  double r = remaining_variables (solver);
  double p = percent (r, solver->max_var);
  column (solver, "variables", 0, 0, 0, r);
  column (solver, "remaining", 0, 1, 0, p);
  column (solver, "log2models", 0, 0, 0, solver->limit.models.log2report-1);
  column (solver, "log2count", 0, 0, 1, solver->limit.count.log2report-1);
  if (!(stats.reports++ % 20)) {
    printf ("c\n");
    for (int i = 0; i < num_report_header_lines; i++) {
      PUSH (HEADER[i], 0);
      printf ("c  %s\n", HEADER[i].start);
    }
    printf ("c\n");
  }
  ENTRIES = 0;
  PUSH (COLUMNS, 0);
  printf ("c %c%s\n", ch, COLUMNS.start);
  inc_model_report_limit (solver);
}

static int
restart_after_first_model_to_split_on_relevant_first (Solver * solver) {
  if (stats.models.counted) return 0;
  if (solver->split_on_relevant_first) return 0;
  return solver->require_to_split_on_relevant_first_after_first_model;
}

static void first_model (Solver * solver) {
  SOG ("saving first model");
  for (int idx = 1; idx <= solver->max_var; idx++) {
    Var * v = var (solver, idx);
    v->first = v->val;
  }
}

static int new_model (Solver * solver) {
  int unassigned = solver->unassigned_relevant_variables;
  stats.models.counted++;
  SOG ("model %ld with %d unassigned relevant variables",
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
      RULE0 (EN0);
      return 0;
    } else if (c->size == 1) {
      SOGCLS (c, "registering unit");
      PUSH (solver->units, c);
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
  SOG ("primal propagation");
  Clause * res = 0;
  while (!res && solver->next.primal < COUNT (solver->trail)) {
    int lit = solver->trail.start[solver->next.primal++];
    if (is_dual_var (var (solver, lit))) continue;
    assert (val (solver, lit) > 0);
    SOG ("primal propagating %d", lit);
    stats.propagated.primal++;
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
      const int other = c->literals[1];
      const int other_val = val (solver, other);
      if (other_val > 0) continue;
      int i = c->search, replacement_val = -1, replacement = 0;
      while (i < c->size) {
	replacement = c->literals[i];
	replacement_val = val (solver, replacement);
	if (replacement_val >= 0) break;
	i++;
      }
      if (replacement_val < 0) {
	i = 2;
	while (i < c->search) {
	  replacement = c->literals[i];
	  replacement_val = val (solver, replacement);
	  if (replacement_val >= 0) break;
	  i++;
	}
      }
      if (replacement_val >= 0) {
	c->search = i;
	SOGCLS (c, "disconnecting literal %d from", -lit);
	c->literals[0] = replacement;
	c->literals[i] = -lit;
	connect_primal_literal (solver, c, replacement);
	q--;
      } else if (!other_val) {
	SOGCLS (c, "forcing %d", other);
	assign (solver, other, c);
        RULE1 (UP, other);
      } else {
	assert (other_val < 0);
	stats.conflicts.primal++;
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

#if 0
static Frame * current_frame (Solver * solver) {
  assert (solver->level < COUNT (solver->frames));
  return solver->frames.start + solver->level;
}

static int next_dual_literal (Solver * solver) {
  if (!solver->level) {
    if (!EMPTY (solver->units.dual)) {
    }
    if (next < COUNT (solver->dual_unit_clauses)) {
      Clause * c = PEEK (solver->dual_unit_clauses, next);
      assert (c->size == 1);
    }
  }
  solver->next.dual < COUNT (solver->trail))
  int lit = solver->trail.start[solver->next.dual++];
  if (is_primal_var (var (solver, lit))) continue;
}
#endif

static void check_primal_propagated (Solver * solver) {
  assert (solver->next.primal == COUNT (solver->trail));
}

static void update_primal_propagated (Solver * solver) {
  while (solver->next.primal < COUNT (solver->trail)) {
    const int lit = PEEK (solver->trail, solver->next.primal);
    if (is_primal_or_shared_var (var (solver, lit))) break;
    solver->next.primal++;
  }
}

static Clause * dual_force (Solver * solver, Clause * c, int lit) {
  assert (!is_primal_var (var (solver, lit)));
  const int tmp = val (solver, lit);
  if (tmp > 0) return 0;
  if (tmp < 0) {
    SOGCLS (c, "conflict");
    stats.conflicts.dual++;
    return c;
  }
  if (is_shared_var (var (solver, lit))) {
    assert (!tmp);
    SOGCLS (c, "shared unit %d", lit);
    stats.units.dual.shared++;
    assume_decision (solver, -lit);
    SOGCLS (c, "conflict");
    stats.conflicts.dual++;
    if (is_relevant_var (var (solver, lit))) RULE1 (UNX, lit);
    else 	                             RULE1 (UNY, lit);
    return c;
  }
  assert (!tmp);
  assert (is_dual_var (var (solver, lit)));
  SOGCLS (c, "dual unit %d", lit);
  assign (solver, lit, c);
  RULE1 (UNT, lit);
  return 0;
}

// Propagate collected dual unit clauses on root level.

static Clause * dual_propagate_units (Solver * solver) {
  assert (!solver->last_decision_level);
  SOG ("dual trail propagation");
  Clause * res = 0;
  while (!res && !EMPTY (solver->units)) {
    Clause * c = POP (solver->units);
    assert (c->dual), assert (c->size == 1);
    const int lit = c->literals[0];
    SOG ("propagating dual unit clause %d", lit);
    assert (!is_primal_var (var (solver, lit)));
    res = dual_force (solver, c, lit);
    if (!res) continue;
    assert (res == c);
    SOGCLS (c, "keeping unit");
    PUSH (solver->units, c);
  }
  if (!res) update_primal_propagated (solver);
  return res;
}

// Propagate assignments on trail through dual CNF.

static Clause * dual_propagate_trail (Solver * solver) {
  SOG ("dual trail propagation");
  check_primal_propagated (solver);
  assert (solver->next.dual <= solver->next.primal);
  Clause * res = 0;
  while (!res && solver->next.dual < COUNT (solver->trail)) {
    const int lit = PEEK (solver->trail, solver->next.dual);
    solver->next.dual++;
    assert (val (solver, lit) > 0);
    if (is_primal_var (var (solver, lit))) continue;
    SOG ("dual propagating %d", lit);
    stats.propagated.dual++;
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
      int i = c->search, replacement_val = -1, replacement = 0;
      while (i < c->size) {
	replacement = c->literals[i];
	replacement_val = val (solver, replacement);
	if (replacement_val >= 0) break;
	i++;
      }
      if (replacement_val < 0) {
	i = 2;
	while (i < c->search) {
	  replacement = c->literals[i];
	  replacement_val = val (solver, replacement);
	  if (replacement_val >= 0) break;
	  i++;
	}
      }
      if (replacement_val >= 0) {
	c->search = i;
	SOGCLS (c, "disconnecting literal %d from", -lit);
	c->literals[0] = replacement;
	c->literals[i] = -lit;
	connect_dual_literal (solver, c, replacement);
	q--;
      } else res = dual_force (solver, c, other);
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
  return res;
}

static int dual_propagating (Solver * solver) {
  if (!solver->dual_solving_enabled) return 0;
  if (!EMPTY (solver->units)) return 1;
  if (solver->split_on_relevant_first) return 1;
  return 0;
}

static Clause * dual_propagate (Solver * solver) {
  if (!dual_propagating (solver)) return 0;
  SOG ("dual propagation");
  check_primal_propagated (solver);
  Clause * res = 0;
  if (!solver->last_decision_level) res = dual_propagate_units (solver);
  if (!res) res = dual_propagate_trail (solver);
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
    type (res), var2idx (solver, res), res->stamp);
  return res;
}

#ifndef NDEBUG

static int is_unit_clause (Solver * solver, Clause * c) {
  int unit = 0;
  for (int i = 0; i < c->size; i++) {
    const int lit = c->literals[i];
    const int tmp = val (solver, lit);
    if (tmp > 0) return 0;
    if (tmp < 0) continue;
    if (unit) return 0;
    unit = lit;
  }
  return unit;
}

static void check_no_empty_clause (Solver * solver) {
  if (!options.check) return;
  for (int dual = 0; dual <= 1; dual++) {
    CNF * cnf = dual ? solver->cnf.dual : solver->cnf.primal;
    if (!cnf) continue;
    for (Clause ** p = cnf->clauses.start; p != cnf->clauses.top; p++)
      assert (!is_unit_clause (solver, *p));
  }
}

static int is_empty_clause (Solver * solver, Clause * c) {
  for (int i = 0; i < c->size; i++) {
    const int lit = c->literals[i];
    const int tmp = val (solver, lit);
    if (tmp >= 0) return 0;
  }
  return 1;
}

static void check_no_unit_clause (Solver * solver) {
  if (!options.check) return;
  for (int dual = 0; dual <= 1; dual++) {
    CNF * cnf = dual ? solver->cnf.dual : solver->cnf.primal;
    if (!cnf) continue;
    for (Clause ** p = cnf->clauses.start; p != cnf->clauses.top; p++)
      assert (!is_empty_clause (solver, *p));
  }
}

static int is_primal_satisfied_no_log (Solver *);

static void check_all_relevant_variables_assigned (Solver * solver) {
  for (int idx = 1; idx <= solver->max_var; idx++) {
    Var * v = var (solver, idx);
    if (!is_relevant_var (v)) continue;
    assert (val (solver, idx));
  }
}

#endif

static void check_dual_propagated (Solver * solver) {
  if (!dual_propagating (solver)) return;
  assert (solver->next.dual == COUNT (solver->trail));
}

static void decide (Solver * solver) {
  check_dual_propagated (solver);
  update_primal_propagated (solver);
  check_primal_propagated (solver);
  assert (!is_primal_satisfied_no_log (solver));
#ifndef NDEBUG
  check_no_empty_clause (solver);
  check_no_unit_clause (solver);
#endif
  Var * v = next_decision (solver);
  int lit = v - solver->vars;
  if (v->phase < 0) lit = -lit;
  SOG ("%s decide %d", type (v), lit);
  assume_decision (solver, lit);
  if (is_relevant_var (v))     RULE1 (DX, lit);
  else {
#ifndef NDEBUG
    if (solver->split_on_relevant_first)
      check_all_relevant_variables_assigned (solver);
#endif
    if (is_irrelevant_var (v)) RULE1 (DY, lit);
    else                       RULE1 (DS, lit);
  }
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

static int
subsumed_learned (Solver * solver, Clause * c, int expected)
{
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
  for (int idx = 1; idx <= solver->max_primal_or_shared_var; idx++)
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

static void flush_dual_garbage_occurrences (Solver * solver) {
  long flushed = 0;
  for (int idx = 1; idx <= solver->max_var; idx++) {
    if (!is_dual_or_shared_var (var (solver, idx))) continue;
    for (int sign = -1; sign <= 1; sign += 2) {
      int lit = sign * idx;
      Clauses * o = dual_occs (solver, lit);
      Clause ** q = o->start, ** p = q;
      while (p < o->top) {
	Clause * c = *p++;
	if (c->size == 1) continue;
	if (c->garbage) flushed++;
	else *q++ = c;
      }
      o->top = q;
      if (EMPTY (*o)) RELEASE (*o);
    }
  }
  SOG ("flushed %ld dual garbage occurrences", flushed);
}

// If a new blocking clause is added, then try to subsume a limited number
// of previously added learned clauses.  This removes some redundant
// blocking clause, but should probably eventually be replaced by a scheme
// which eagerly and in a more controlled way removes blocking clauses.

static void subsume_learned (Solver * solver, Clause * c) {
  if (!options.subsume) return;
  if (!options.sublearned) return;
  if (c->size <= 1) return;
  assert (!c->redundant);
  int limit = MAX (options.sublearnlim, 0);
  SOG ("trying to subsume at most %d learned clauses", limit);
  assert (!c->dual);
  mark_clause (solver, c);
  Clause ** p = solver->cnf.primal->clauses.top;
  int count = 0;
  while (p != solver->cnf.primal->clauses.start) {
    Clause * d = *--p;
    if (d == c) continue;
    if (d->garbage) break;
    if (subsumed_learned (solver, d, c->size)) {
      SOGCLS (d, "subsumed");
      d->garbage = 1;
      stats.blocked.subsumed++;
      count++;
    } else if (!limit--) break;
  }
  unmark_clause (solver, c);
  SOG ("subsumed %d clauses", count);
  if (stats.subsumed <= solver->limit.subsumed) return;
  flush_primal_garbage_occurrences (solver);
  collect_garbage_clauses (solver->cnf.primal);
  set_subsumed_learned_limit (solver);
  report (solver, 1, 's');
}

static void adjust_next_to_trail (Solver * solver) {
  adjust_next (solver, COUNT (solver->trail));
}

static void register_new_fixed_variable (Solver * solver) {
  if (solver->last_decision_level > 0) return;
  SOG ("registered new fixed variable");
  solver->found_new_fixed_variable = 1;
}

static void add_decision_blocking_clause (Solver * solver, long * rule) {
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
      type (var (solver, decision)), -decision, level);
  }
  const int size = COUNT (solver->clause);
  stats.blocked.literals += size;
  SOG ("found blocking clause of length %d", size);
  assert (size <= 1 || solver->clause.start[0] == -first);
  int other;
  for (;;) {
    other = TOP (solver->trail);
    (void) POP (solver->trail);
    if (unassign (solver, other) != UNDECIDED) dec_level (solver);
    if (other == first) break;
  }
  Clause * c = new_clause (solver->clause.start, size);
  assert (!c->glue), assert (!c->redundant);
  SOGCLS (c, "decision blocking");
  add_clause_to_cnf (c, solver->cnf.primal);
  if (size > 1) connect_primal_clause (solver, c);
  CLEAR (solver->clause);
  adjust_next_to_trail (solver);
  assign (solver, -first, c);
       if (rule == &rules.BP1L) RULE2 (BP1L, -first, c);
  else if (rule == &rules.BN0L) RULE2 (BN0L, -first, c);
  else SOG ("WARNING UNMATCHED RULE in '%s'", __FUNCTION__);
  register_new_fixed_variable (solver);
  subsume_learned (solver, c);
}

static int blocking_clause_size (Solver * solver, int target_level) {
  assert (0 < target_level);
  assert (target_level <= solver->level);
  assert (!frame_at_level (solver, target_level)->flipped);
  int size = solver->num_decision_levels, level;
  for (level = solver->last_decision_level;
       level > target_level;
       level = frame_at_level (solver, level)->prev.decision)
    size--;
  assert (level == target_level);
  assert (size > 0);
  return size;
}

static int blocking (Solver * solver, int target_level) {
  if (!options.block) return 0;
  int size = blocking_clause_size (solver, target_level);
  int limit = options.blocklimit;
  int res = (size <= limit);
  if (res) SOG ("blocking clause size %d below limit %d", size, limit);
  else SOG ("blocking clause size %d would exceed limit %d", size, limit);
  return res;
}

/*------------------------------------------------------------------------*/

static void
check_no_relevant_decision_above_level (Solver * solver, int level) {
#ifndef NDEBUG
  for (int i = level + 1; i <= solver->level; i++) {
    Frame * f = frame_at_level (solver, i);
    if (f->flipped) continue;
    const int decision = f->decision;
    Var * v = var (solver, decision);
    assert (!is_relevant_var (v));
  }
#endif
}

static void
check_no_decision_above_level (Solver * solver, int level) {
#ifndef NDEBUG
  for (int i = level + 1; i <= solver->level; i++) {
    Frame * f = frame_at_level (solver, i);
    assert (f->flipped);
  }
#endif
}

static int is_relevant_decision_level (Solver * solver, int level) {
  if (!level) return 0;
  Frame * f = frame_at_level (solver, level);
  if (f->flipped) return 0;
  Var * v = var (solver, f->decision);
  return is_relevant_var (v);
}

/*------------------------------------------------------------------------*/

// Without the dual part we find models as in a standard CDCL solvers, i.e.,
// if all variables are propagated in the primal part without getting a
// conflict then the formula is supposed to be satisfied.

static int is_primal_satisfied_no_log (Solver * solver) {
  if (solver->next.primal < COUNT (solver->trail)) return 0;
  return !solver->unassigned_primal_and_shared_variables;
}

static void check_primal_satisfied (Solver * solver) {
  assert (is_primal_satisfied_no_log (solver));
}

static int is_primal_satisfied (Solver * solver) {
  int res = is_primal_satisfied_no_log (solver);
  if (res) SOG ("all primal and shared variables assigned");
  return res;
}

#ifndef NLOG

static int last_relevant_decision (Solver * solver) {
  assert (solver->last_relevant_level > 0);
  Frame * f = frame_at_level (solver, solver->last_relevant_level);
  assert (!f->flipped);
  int res = f->decision;
  assert (is_relevant_var (var (solver, res)));
  return res;
}

#endif

static void force_splitting_on_relevant_first (Solver *);

static void backtrack (Solver * solver, int level) {
  SOG ("backtrack to level %d", level);
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    if (v->level == level) break;
    const Decision decision = unassign (solver, lit);
    if (decision != UNDECIDED) dec_level (solver);
    (void) POP (solver->trail);
  }
  adjust_next_to_trail (solver);
}

static void
backtrack_primal_satisfied_learn (Solver * solver, int level) {
  SOG ("applying primal satisfied learning rule");

  check_primal_satisfied (solver);
  assert (is_relevant_decision_level (solver, level));
  check_no_relevant_decision_above_level (solver, level);

  backtrack (solver, level);
  add_decision_blocking_clause (solver, &rules.BP1L);
}

static void initialize_count (Solver * solver, int level, int counted) {
  Frame * f = frame_at_level (solver, level);
  add_power_of_two_to_number (f->count, counted);
  f->counted++;
  SOGNUM (f->count,
    "initialized level %d flipping count to 2^%d =", level, counted);
}

static void
backtrack_accumulating_flipped_counts (Solver * solver, int level) {
  Frame * f = frame_at_level (solver, level);
  int lit, decision = decision_at_level (solver, level);
  while ((lit = TOP (solver->trail)) != decision) {
    const Decision type = unassign (solver, lit);
    if (type == DECISION) dec_level (solver);
    else if (type == FLIPPED) {
      Frame * g = last_frame (solver);
      assert (g->decision == lit);
      SOGNUM (g->count, "accumulating level %d flipping count", level);
      add_number (f->count, g->count);
      f->counted += g->counted;
      dec_level (solver);
    }
    (void) POP (solver->trail);
  }
  SOGNUM (f->count, "final level %d flipping count", level);
}

static void
backtrack_primal_satisfied_flip (Solver * solver, int level, int counted)
{
  SOG ("applying primal satisfied flipping rule");

  assert (counted >= 0);
  check_primal_satisfied (solver);
  assert (is_relevant_decision_level (solver, level));
  check_no_relevant_decision_above_level (solver, level);

  initialize_count (solver, level, counted);
  backtrack_accumulating_flipped_counts (solver, level);

  flip_decision (solver, &rules.BP1F);
  register_new_fixed_variable (solver);
  adjust_next_to_trail (solver);
}

static int backtrack_primal_satisfied (Solver * solver) {
  if (restart_after_first_model_to_split_on_relevant_first (solver)) {
    force_splitting_on_relevant_first (solver);
    return 1;
  }
  SOG ("backtrack primal satisfied");
  check_primal_satisfied (solver);
  int counted;
  if (last_model (solver, &counted)) return 0;
  if (!solver->last_relevant_level) {
    SOG ("satisfied without any relevant decisions on the trail");
    check_no_relevant_decision_above_level (solver, 0);
    RULE0 (EP1);
    return 0;
  }
  stats.back.tracked++;
  int level = solver->last_relevant_level;
  SOG ("backtracking to last relevant decision %d at level %d",
    last_relevant_decision (solver), level);
  if (blocking (solver, level))
    backtrack_primal_satisfied_learn (solver, level);
  else backtrack_primal_satisfied_flip (solver, level, counted);
  return 1;
}

/*------------------------------------------------------------------------*/

static int resolve_literal (Solver * solver, int lit) {
  Var * v = var (solver, lit);
  if (!v->level) return 0;
  if (v->seen) return 0;
  v->seen = 1;
  PUSH (solver->seen, v);
  SOG ("%s seen literal %d", type (v), lit);
  assert (val (solver, lit) < 0);
  Frame * f = frame (solver, lit);
  if (!f->seen) {
    SOG ("seen level %d", v->level);
    PUSH (solver->levels, v->level);
    f->seen = 1;
  }
  if (v->level == solver->level) return 1;
  SOG ("%s adding literal %d", type (v), lit);
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
  SOG ("%s bump variable %d", type (v), var2idx (solver, v));
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
    SOG ("also marking %s reason variable %d as seen", type (v), idx);
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
  const long n = stats.conflicts.primal;
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

static Clause * learn_primal_clause (Solver * solver, int glue, int level) {
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
  Frame * f = solver->frames.start + solver->level;
  assert (f->flipped);
  if (is_zero_number (f->count)) return;
  assert (f->counted > 0);
  stats.models.discounted++;
  SOGNUM (f->count, "discounted actual models");
  sub_number (solver->count, f->count);
  if (solver->model_printing_enabled) print_discount (solver, f);
  report (solver, 3, 'd');
}

static void
backjump_primal_conflict_learn (Solver * solver, Clause * c, int level) {
  assert (c->size > 0);
  const int forced = c->literals[0];
  stats.back.jumped++;
  SOG ("back-jump %ld to level %d", stats.back.jumped, level);
  while (!EMPTY (solver->trail)) {
    const int lit = TOP (solver->trail);
    Var * v = var (solver, lit);
    if (v->level == level) break;
    (void) POP (solver->trail);
    const Decision decision = unassign (solver, lit);
    if (decision == UNDECIDED) continue;
    if (decision == FLIPPED) discount (solver);
    dec_level (solver);
  }
  assert (!val (solver, forced));
  assert (solver->level == level);
  adjust_next_to_trail (solver);
  assign (solver, forced, c);
  RULE2 (JP0, forced, c);
  register_new_fixed_variable (solver);
}

/*------------------------------------------------------------------------*/

static void
backtrack_primal_conflict_flip (Solver * solver, int level) {

  SOG ("applying primal conflict flipping rule at level %d", level);
  stats.back.tracked++;

  assert (level);
  assert (level == solver->last_decision_level);
  check_no_decision_above_level (solver, level);

  backtrack_accumulating_flipped_counts (solver, level);

  flip_decision (solver, &rules.BP0F);
  register_new_fixed_variable (solver);
  adjust_next_to_trail (solver);
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
    SOG ("%s resolving literal %d", type (var (solver, uip)), uip);
    c = var (solver, uip)->reason;
  }
  SOG ("%s first UIP literal %d", type (var (solver, uip)), uip);
  PUSH (solver->clause, -uip);
  if (options.bump) bump_seen (solver);
  reset_seen (solver);
  return reset_levels (solver);
}

static long flipped_levels_above (Solver * solver, int level) {
  long res = 0;
  int flipped = 0;
  for (int i = level + 1; i <= solver->level; i++) {
    Frame * f = frame_at_level (solver, i);
    if (f->flipped) flipped++, res += f->counted;
  }
  SOG ("%d flipped levels above %d counted %ld", flipped, level, res);
  return res;
}

static int analyze_primal_conflict (Solver * solver, Clause * conflict) {

  SOG ("analyze primal");
  assert (conflict);
  assert (!conflict->dual);

  if (!solver->last_decision_level) {
    SOG ("primal conflict without any decisions on the trail");
    check_no_decision_above_level (solver, 0);
    RULE0 (EP0);
    return 0;
  }

  // Resolve conflict in any case, since this also bumps variables.
  //
  int glue = resolve_primal_conflict (solver, conflict);

  // Also determine back-jump level even if we only backtrack.
  //
  int level = jump_level (solver);
  long discount = flipped_levels_above (solver, level);

  int learn;	// Learn a clause or just backtrack and flip.

  if (!options.learn) {
    SOG ("since learning disabled just backtrack and flip");
    level = solver->last_decision_level;
    learn = 0;
  } else if (solver->last_flipped_level <= level) {
    assert (!discount);
    SOG ("learn since flipped level %d less equal back-jump level %d",
      solver->last_flipped_level, level);
    learn = 1;
  } else if (!options.discount) {
    stats.back.forced++;
    SOG ("forces backtracking and flipping since discounting is disabled");
    level = solver->last_decision_level;
    learn = 0;
  } else if (discount > options.discountmax) {
    level = solver->last_decision_level;
    learn = 0;
  } else {
    assert (0 <= discount);
    assert (discount <= options.discountmax);
    SOG ("discounting %ld levels above back-jump level %d", discount, level);
    if (discount > 0) stats.back.discounting++;
    learn = 1;
  }

  if (learn) {
    Clause * c = learn_primal_clause (solver, glue, level);
    backjump_primal_conflict_learn (solver, c, level);
  } else {
    CLEAR (solver->clause);
    backtrack_primal_conflict_flip (solver, level);
  }

  return 1;
}

/*------------------------------------------------------------------------*/

static void
backtrack_dual_conflict_learn (Solver * solver, int level) {
  SOG ("applying dual conflict learning rule");

  assert (is_relevant_decision_level (solver, level));
  check_no_relevant_decision_above_level (solver, level);

  backtrack (solver, level);
  add_decision_blocking_clause (solver, &rules.BN0L);
}

static void
backtrack_dual_conflict_flip (Solver * solver, int level, int counted)
{
  SOG ("applying dual conflict flipping rule");

  assert (counted >= 0);
  assert (is_relevant_decision_level (solver, level));
  check_no_relevant_decision_above_level (solver, level);

  initialize_count (solver, level, counted);
  backtrack_accumulating_flipped_counts (solver, level);

  flip_decision (solver, &rules.BN0F);
  register_new_fixed_variable (solver);
  adjust_next_to_trail (solver);
}

/*------------------------------------------------------------------------*/

// Basic backtracking version without actual analysis part yet.

static int analyze_dual_conflict (Solver * solver, Clause * conflict) {
  if (restart_after_first_model_to_split_on_relevant_first (solver)) {
    force_splitting_on_relevant_first (solver);
    return 1;
  }
  SOG ("analyze dual");
  assert (conflict);
  assert (conflict->dual);
  int counted;
  if (last_model (solver, &counted)) return 0;
  if (!solver->last_relevant_level) {
    SOG ("dual conflict without any relevant decisions left");
    check_no_relevant_decision_above_level (solver, 0);
    RULE0 (EN0);
    return 0;
  }
  stats.back.tracked++;
  int level = solver->last_relevant_level;
  SOG ("backtracking to last relevant decision %d at level %d",
    last_relevant_decision (solver), level);
  if (blocking (solver, level))
    backtrack_dual_conflict_learn (solver, level);
  else backtrack_dual_conflict_flip (solver, level, counted);
  return 1;
}

/*------------------------------------------------------------------------*/

static int primal_reducing (Solver * solver) {
  if (solver->primal_or_shared_fixed >
      solver->limit.reduce.primal_or_shared_fixed) return 1;
  if (!options.reduce) return 0;
  return stats.learned > solver->limit.reduce.learned;
}

static int dual_reducing (Solver * solver) {
  if (!solver->cnf.dual) return 0;
  if (solver->dual_or_shared_fixed >
      solver->limit.reduce.dual_or_shared_fixed) return 1;
  return 0;
}

static int reducing (Solver * solver) {
  return primal_reducing (solver) || dual_reducing (solver);
}

static void inc_reduce_limit (Solver * solver) {
  if (!stats.conflicts.primal) return;
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

static void reduce_primal (Solver * solver)
{
  if (!primal_reducing (solver)) return;

  const int simplify =
    solver->primal_or_shared_fixed >
      solver->limit.reduce.primal_or_shared_fixed;

  SOG ("primal %sreduction", simplify ? "simplifying " : "");

  Clauses candidates;
  INIT (candidates);

  CNF * primal = solver->cnf.primal;

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
    RULE0 (FP);
    marked++;
  }

  RELEASE (candidates);
  SOG ("marked %ld primal clauses as garbage", marked);

  flush_primal_garbage_occurrences (solver);
  collect_garbage_clauses (primal);

  solver->limit.reduce.primal_or_shared_fixed =
    solver->primal_or_shared_fixed;
}

static void reduce_dual (Solver * solver)
{
  if (!dual_reducing (solver)) return;

  CNF * dual = solver->cnf.dual;
  assert (dual);

  const int simplify =
    solver->dual_or_shared_fixed >
      solver->limit.reduce.dual_or_shared_fixed;

  if (!simplify) return;

  SOG ("dual simplifying reduction");

  for (Clause ** p = dual->clauses.start;
       p < dual->clauses.top;
       p++) {
    Clause * c = *p;
    if (c->garbage) continue;
    (void) mark_satisfied_as_garbage (solver, c);
  }

  flush_dual_garbage_occurrences (solver);
  collect_garbage_clauses (dual);

  solver->limit.reduce.dual_or_shared_fixed =
    solver->dual_or_shared_fixed;
}

static void reduce (Solver * solver) {
  stats.reductions++;
  SOG ("reduction %ld", stats.reductions);

  reduce_primal (solver);
  reduce_dual (solver);

  inc_reduce_limit (solver);
  report (solver, 1, '-');
}

/*------------------------------------------------------------------------*/

static void inc_restart_limit (Solver * solver) {
  const int inc = MAX (options.restartint, 1);
  solver->limit.restart.conflicts = stats.conflicts.primal + inc;
  SOG ("new restart conflicts limit %ld",
    solver->limit.restart.conflicts);
}

static int restarting (Solver * solver) {
  if (!options.restart) return 0;
  if (solver->last_flipped_level) return 0;
  if (!solver->level) return 0;
  if (stats.conflicts.primal <= solver->limit.restart.conflicts) return 0;
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

static void force_splitting_on_relevant_first (Solver * solver) {
  assert (!solver->split_on_relevant_first);
  SOG ("forcing to split on relevant variables first");
  solver->split_on_relevant_first = 1;
  int level = 0;
  while (level + 1 <= solver->level &&
         is_relevant_decision_level (solver, level))
    level++;
  if (level < solver->level) backtrack (solver, level);
}

static void restart (Solver * solver) {
  const int level = reuse_trail (solver);
  stats.restarts++;
  SOG ("restart %d", stats.restarts);
  backtrack (solver, level);
  report (solver, 2, 'r');
}

static void solve (Solver * solver) {
  if (model_limit_reached (solver)) return;
  if (!connect_primal_cnf (solver)) return;
  if (primal_propagate (solver)) return;
  if (is_primal_satisfied (solver)) { (void) new_model (solver); return; }
  if (!connect_dual_cnf (solver)) { (void) new_model (solver); return; }
  for (;;) {
    Clause * conflict = primal_propagate (solver);
    if (conflict) {
      if (!analyze_primal_conflict (solver, conflict)) return;
    } else if (is_primal_satisfied (solver)) {
      if (!backtrack_primal_satisfied (solver)) return;
    } else {
      conflict = dual_propagate (solver);
      if (conflict) {
	if (!analyze_dual_conflict (solver, conflict)) return;
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
  assert (solver->dual_solving_enabled);
  limit_number_of_partial_models (solver, 1);
  solve (solver);
  return stats.models.counted ? 10 : 20;
}

void primal_count (Number models, Solver * solver) {
  assert (!solver->dual_solving_enabled);
  solve (solver);
  copy_number (models, solver->count);
}

void dual_count (Number models, Solver * solver) {
  assert (solver->dual_solving_enabled);
  solve (solver);
  copy_number (models, solver->count);
}

void primal_enumerate (Solver * solver, Name name) {
  assert (!solver->dual_solving_enabled);
  enable_model_printing (solver, name);
  solve (solver);
}

void dual_enumerate (Solver * solver, Name name) {
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
  for (int level = 0; level <= solver->level; level++) {
    int start = frame_at_level (solver, level)->trail, end;
    if (level == solver->level) end = (int) COUNT (solver->trail);
    else end = frame_at_level (solver, level+1)->trail;
    printf ("frame %d :", level);
    for (int i = start; i < end; i++) {
      int lit = PEEK (solver->trail, i);
      printf (" %d", lit);
      Var * v = var (solver, lit);
      if (v->decision == UNDECIDED) fputc ('p', stdout);
      if (v->decision == DECISION) fputc ('d', stdout);
      if (v->decision == FLIPPED) fputc ('f', stdout);
    }
    fputc ('\n', stdout);
  }
}

#endif
