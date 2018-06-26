#include "headers.h"

typedef struct Sub Sub;

struct Sub {
  CNF * cnf;
  int max_var;
  signed char * marks;
  long original, subsumed, strengthened;
  Clauses * occs;
};

static Sub * new_subsume (CNF * cnf) {
  Sub * sub;
  NEW (sub);
  sub->cnf = cnf;
  sub->max_var = maximum_variable_index (cnf);
  LOG ("maximum variable index %d", sub->max_var);
  ALLOC (sub->marks, sub->max_var + 1);
  ALLOC (sub->occs, 2*(sub->max_var + 1));
  sub->occs += sub->max_var;
  return sub;
}

static void delete_subsume (Sub * sub) {
  DEALLOC (sub->marks, sub->max_var + 1);
  for (int lit = -sub->max_var; lit <= sub->max_var; lit++)
    RELEASE (sub->occs[lit]);
  sub->occs -= sub->max_var;
  DEALLOC (sub->occs, 2*(sub->max_var + 1));
  DELETE (sub);
}

static int try_to_subsume_clause (Sub * sub, Clause * c) {
  LOGCLS (c, "candidate");

  int sign (int lit) { return lit < 0 ? -1 : 1; }

  void mark (int lit) {
    const int idx = abs (lit);
    assert (!sub->marks[idx]);
    sub->marks[idx] = sign (lit);
  }

  int marked (int lit) {
    const int idx = abs (lit);
    int res = sub->marks[idx];
    if (lit < 0) res = -res;
    return res;
  }

  void unmark (int lit) { sub->marks[abs (lit)] = 0; }
  
  for (int i = 0; i < c->size; i++)
    mark (c->literals[i]);

  for (int i = 0; i < c->size; i++)

    unmark (c->literals[i]);

  return 0;
}

void subsume_clauses (CNF * cnf) {
  if (!options.subsume) return;
#ifndef NLOG
  const char * type = cnf->dual ? "dual" : "primal";
  LOG ("%s subsumed clause elimination", type);
#endif
  Sub * sub = new_subsume (cnf);
  Clauses clauses;
  INIT (clauses);
  for (Clause ** p = cnf->clauses.start; p != cnf->clauses.top; p++) {
    Clause * c = *p;
    if (c->garbage) continue;
    if (c->redundant &&
        c->size > options.keepsize &&
        c->glue > options.keepglue) continue;
    PUSH (clauses, c);
  }
  LOG ("found %zd candidate clauses", COUNT (clauses));
  int cmp (const void * p, const void * q) {
    Clause * c = * (Clause **) p, * d = * (Clause **) q;
    if (c->size < d->size) return -1;
    if (c->size > d->size) return 1;
    assert (c->size == d->size);
    if (!c->redundant && d->redundant) return -1;
    if (c->redundant && !d->redundant) return 1;
    assert (c->redundant == d->redundant);
    if (c->id < d->id) return -1;
    if (c->id > d->id) return 1;
    return 0;
  }
  long changed;
  do {
    changed = 0;
    qsort (clauses.start, COUNT (clauses), sizeof (Clause *), cmp);
    for (Clause ** p = clauses.start; p != clauses.top; p++)
      changed += try_to_subsume_clause (sub, *p);
  } while (changed);
  RELEASE (clauses);
  msg (1, "subsumed %ld clauses %.0f%% out of %d",
    sub->subsumed, type,
    percent (sub->subsumed, sub->original), sub->original);
  msg (1, "strengthened %ld clauses %.0f%% out of %d",
    sub->strengthened, type,
    percent (sub->strengthened, sub->original), sub->original);
  delete_subsume (sub);
}
