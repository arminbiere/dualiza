#include "headers.h"

typedef struct Sub Sub;

struct Sub {
  CNF * cnf;
  Clause * empty;
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
  ALLOC (sub->occs, sub->max_var + 1);
  sub->original = cnf->irredundant + cnf->redundant;
  return sub;
}

static void delete_subsume (Sub * sub) {
  DEALLOC (sub->marks, sub->max_var + 1);
  for (int idx = 1; idx <= sub->max_var; idx++)
    RELEASE (sub->occs[idx]);
  DEALLOC (sub->occs, sub->max_var + 1);
  DELETE (sub);
}

static int sign (int lit) { return lit < 0 ? -1 : 1; }

static void mark (Sub * sub, int lit) {
  const int idx = abs (lit);
  assert (!sub->marks[idx]);
  sub->marks[idx] = sign (lit);
}

static int marked (Sub * sub, int lit) {
  const int idx = abs (lit);
  int res = sub->marks[idx];
  if (lit < 0) res = -res;
  return res;
}

static void unmark (Sub * sub, int lit) { sub->marks[abs (lit)] = 0; }

static void
subsume_clause (Sub * sub, Clause * subsuming, Clause * subsumed)
{
  assert (!subsumed->garbage);
  assert (!subsuming->garbage);
  assert (!subsuming->redundant || subsumed->redundant);
  LOGCLS (subsumed, "subsumed");
  LOGCLS (subsuming, "subsuming");
  stats.subsumed++;
  sub->subsumed++;
  subsumed->garbage = 1;
}

static void
strengthen_clause (Sub * sub,
  Clause * strengthening, Clause * strengthened, int remove)
{
  assert (!strengthened->garbage);
  LOGCLS (strengthened, "removing %d in", remove);
  LOGCLS (strengthening, "strengthening");
  int j = 0, found = 0;
  for (int i = 0; i < strengthened->size; i++) {
    const int lit = strengthened->literals[i];
    if (lit == remove) assert (!found), found = 1;
    else strengthened->literals[j++] = lit;
  }
  assert (j == strengthened->size - 1);
  strengthened->size = j;
  DEC_ALLOCATED (sizeof (int));
  LOGCLS (strengthened, "strengthened");
  assert (found), (void) found;
  stats.strengthened++;
  sub->strengthened++;
}

static int try_to_subsume_clause (Sub * sub, Clause * c) {

  if (c->garbage) return 0;

  stats.tried++;
  LOGCLS (c, "candidate");
  assert (!c->garbage);

  for (int i = 0; i < c->size; i++)
    mark (sub, c->literals[i]);

  int res = 0, min_idx = 0, negated = 0;
  size_t min_occs = ~(size_t)0;
  Clause * d = 0;

  for (int i = 0; !res && i < c->size; i++) {
    const int lit = c->literals[i], idx = abs (lit);
    Clauses * clauses = sub->occs + idx;
    const size_t tmp_occs = COUNT (*clauses);
    if (tmp_occs < min_occs) min_occs = tmp_occs, min_idx = idx;
    for (Clause ** p = clauses->start; !res && p != clauses->top; p++) {
      d = *p;
      if (d->garbage) continue;
      assert (d->size <= c->size);
      if (c->redundant && !d->redundant) continue;
      negated = 0;
      int j;
      for (j = 0; j < d->size; j++) {
	const int other = d->literals[j];
	const int tmp = marked (sub, other);
	if (tmp > 0) continue;
	if (!tmp) break;
	assert (tmp < 0);
	if (negated) break;
	negated = other;
      }
      if (j < d->size) continue;
      if (negated) res = negated;	// strengthen
      else res = INT_MIN;		// subsume
    }
  }

  for (int i = 0; i < c->size; i++)
    unmark (sub, c->literals[i]);

  if (res == INT_MIN) {
    subsume_clause (sub, d, c);
    assert (c->garbage);
    return 0;
  }

  if (res) {
    strengthen_clause (sub, d, c, -negated);
    min_occs = ~(size_t) 0, min_idx = 0;
    for (int i = 0; i < c->size; i++) {
      const int lit = c->literals[i], idx = abs (lit);
      Clauses * clauses = sub->occs + idx;
      const size_t tmp_occs = COUNT (*clauses);
      if (tmp_occs >= min_occs) continue;
      min_occs = tmp_occs, min_idx = idx;
    }
  }

  if (min_idx) {
    LOG ("minimum occurrence variable %d with %zd occurrences",
      min_idx, min_occs);
    PUSH (sub->occs[min_idx], c);
  } else {
    LOG ("found empty clause during strengthening");
    assert (!c->size);
    sub->empty = c;
  }

  return 0;
}

static int cmp (const void * p, const void * q) {
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

int subsume_clauses (CNF * cnf) {
  if (!options.subsume) return 1;
  const char * type = cnf->dual ? "dual" : "primal";
  LOG ("%s subsumed clause elimination", type);
  Sub * sub = new_subsume (cnf);
  Clauses clauses;
  INIT (clauses);
  for (Clause ** p = cnf->clauses.start; p != cnf->clauses.top; p++) {
    Clause * c = *p;
    if (!sub->empty && c->size == 0) { sub->empty = c; break; }
    if (c->garbage) continue;
    if (c->redundant &&
        c->size > options.keepsize &&
        c->glue > options.keepglue) continue;
    PUSH (clauses, c);
  }
  LOG ("found %zd candidate clauses", COUNT (clauses));
  long strengthened = 1;
  for (int round = 1; strengthened && !sub->empty; round++) {
    strengthened = 0;
    qsort (clauses.start, COUNT (clauses), sizeof (Clause *), cmp);
    for (Clause ** p = clauses.start; p != clauses.top; p++)
      strengthened += try_to_subsume_clause (sub, *p);
    for (int idx = 1; idx <= sub->max_var; idx++)
      CLEAR (sub->occs[idx]);
    msg (3, "strengthened %ld clauses in subsumption round %d",
      strengthened, round);
  }
  int res = !sub->empty;
  if (sub->empty)
    msg (1, "found empty clause during clause subsumption");
  if (sub->subsumed)
    msg (2, "subsumed %ld %s clauses %.0f%% out of %d",
      sub->subsumed, type,
      percent (sub->subsumed, sub->original), sub->original);
  if (sub->strengthened)
    msg (2, "strengthened %ld %s clauses %.0f%% out of %d",
      sub->strengthened, type,
      percent (sub->strengthened, sub->original), sub->original);
  RELEASE (clauses);
  delete_subsume (sub);
  return res;
}
