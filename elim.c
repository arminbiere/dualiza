#include "headers.h"

typedef struct Elm Elm;

struct Elm
{
  CNF *cnf;
  int frozen, max_var, original, eliminated;
  signed char *marks;
  IntStack schedule;
  IntStack clause;
  Clauses *occs;
  int *score;
};

static void
score_elimination_clause (Elm * elm, Clause * c)
{
  if (c->garbage)
    return;
  assert (!c->redundant);
  assert (elm->cnf->dual == c->dual);
  for (int i = 0; i < c->size; i++)
    {
      const int lit = c->literals[i], idx = abs (lit);
      if (idx <= elm->frozen)
	continue;
      if (c->size > options.elimclslim)
	elm->score[idx] = INT_MAX;
      else if (elm->score[idx] < INT_MAX)
	elm->score[idx]++;
    }
}

static void
connect_elimination_clause (Elm * elm, Clause * c)
{
  if (c->garbage)
    return;
  assert (!c->redundant);
  assert (elm->cnf->dual == c->dual);
  for (int i = 0; i < c->size; i++)
    {
      const int lit = c->literals[i], idx = abs (lit);
      if (idx <= elm->frozen)
	continue;
      if (elm->score[idx] == INT_MAX)
	continue;
      PUSH (elm->occs[lit], c);
    }
}

static int
original_non_frozen_variables (Elm * elm)
{
  Clauses *clauses = &elm->cnf->clauses;
  int res = 0;
  for (Clause ** p = clauses->start; p != clauses->top; p++)
    {
      Clause *c = *p;
      if (c->garbage)
	continue;
      for (int i = 0; i < c->size; i++)
	{
	  const int lit = c->literals[i], idx = abs (lit);
	  if (idx <= elm->frozen)
	    continue;
	  if (elm->marks[idx])
	    continue;
	  elm->marks[idx] = 1;
	  res++;
	}
    }
  memset (elm->marks, 0, elm->max_var + 1);
  LOG ("found %d original non-frozen variables", res);
  return res;
}

static Elm *
new_elimination (CNF * cnf, int frozen)
{
  Elm *elm;
  NEW (elm);
  elm->cnf = cnf;
  elm->frozen = frozen;
  elm->max_var = maximum_variable_index (cnf);
  LOG ("found maximum variable index %d", elm->max_var);
  ALLOC (elm->occs, 2 * (elm->max_var + 1));
  elm->occs += elm->max_var;
  ALLOC (elm->score, elm->max_var + 1);
  ALLOC (elm->marks, elm->max_var + 1);
  elm->original = original_non_frozen_variables (elm);
  return elm;
}

static void
delete_elimination (Elm * elm)
{
  for (int i = -elm->max_var; i <= elm->max_var; i++)
    RELEASE (elm->occs[i]);
  elm->occs -= elm->max_var;
  DEALLOC (elm->occs, 2 * (elm->max_var + 1));
  DEALLOC (elm->score, elm->max_var + 1);
  DEALLOC (elm->marks, elm->max_var + 1);
  RELEASE (elm->schedule);
  RELEASE (elm->clause);
  DELETE (elm);
}

static int
elimination_occurrences (Elm * elm, int lit)
{
  Clauses *clauses = elm->occs + lit;
  Clause **q = clauses->start;
  for (Clause ** p = q, *c; p != clauses->top; p++)
    if (!(c = *p)->garbage)
      *q++ = c;
  clauses->top = q;
  int res = q - clauses->start;
  LOG ("literal %d occurs %d times", lit, res);
  return res;
}

static int
sign (int lit)
{
  return lit < 0 ? -1 : 1;
}

static void
mark (Elm * elm, int lit)
{
  const int idx = abs (lit);
  assert (!elm->marks[idx]);
  elm->marks[idx] = sign (lit);
}

static int
marked (Elm * elm, int lit)
{
  const int idx = abs (lit);
  int res = elm->marks[idx];
  if (lit < 0)
    res = -res;
  return res;
}

static void
unmark (Elm * elm, int lit)
{
  elm->marks[abs (lit)] = 0;
}

static int
eliminate_variable (Elm * elm, int pivot)
{

  int m = elimination_occurrences (elm, pivot);
  int n = elimination_occurrences (elm, -pivot);

  int limit = m + n;
  if (!limit)
    return 0;
  if (limit > options.elimocclim)
    return 0;

  LOG ("trying to eliminate %d", pivot);
  assert (pivot > elm->frozen);
  stats.pivots++;

  int resolvents = 0;

  Clauses *pos = elm->occs + pivot;
  Clauses *neg = elm->occs - pivot;

  for (Clause ** p = pos->start; p != pos->top; p++)
    {
      Clause *c = *p;
      assert (!c->garbage);
      for (int i = 0; i < c->size; i++)
	mark (elm, c->literals[i]);
      for (Clause ** q = neg->start; q != neg->top; q++)
	{
	  Clause *d = *q;
	  assert (!d->garbage);
	  int tautological = 0;
	  for (int j = 0; !tautological && j < d->size; j++)
	    {
	      const int lit = d->literals[j];
	      if (lit == -pivot)
		continue;
	      assert (lit != pivot);
	      tautological = (marked (elm, lit) < 0);
	    }
	  if (tautological)
	    continue;
	  if (++resolvents > limit)
	    break;
	}
      for (int i = 0; i < c->size; i++)
	unmark (elm, c->literals[i]);
      if (resolvents > limit)
	break;
    }
  stats.resolutions += resolvents;

  if (resolvents > limit)
    {
      LOG ("can not eliminate variable %d (needs more than %d resolvents)",
	   pivot, limit);
      return 0;
    }

  LOG ("eliminating variable %d", pivot);
  elm->eliminated++;
  stats.eliminated++;

  for (Clause ** p = pos->start; p != pos->top; p++)
    {
      Clause *c = *p;
      assert (!c->garbage);
      for (int i = 0; i < c->size; i++)
	mark (elm, c->literals[i]);
      for (Clause ** q = neg->start; q != neg->top; q++)
	{
	  Clause *d = *q;
	  assert (!d->garbage);
	  int tautological = 0;
	  for (int j = 0; !tautological && j < d->size; j++)
	    {
	      const int lit = d->literals[j];
	      if (lit == -pivot)
		continue;
	      assert (lit != pivot);
	      tautological = (marked (elm, lit) < 0);
	    }
	  if (tautological)
	    continue;

	  CLEAR (elm->clause);
	  LOGCLS (c, "1st antecedent");
	  assert (!c->redundant);
	  assert (!c->garbage);
	  for (int i = 0; i < c->size; i++)
	    {
	      const int lit = c->literals[i];
	      if (lit == pivot)
		continue;
	      assert (lit != -pivot);
	      assert (marked (elm, lit) > 0);
	      PUSH (elm->clause, lit);
	    }
	  LOGCLS (d, "2nd antecedent");
	  assert (!d->redundant);
	  assert (!d->garbage);
	  for (int i = 0; i < d->size; i++)
	    {
	      const int lit = d->literals[i];
	      if (lit == -pivot)
		continue;
	      assert (lit != pivot);
	      int tmp = marked (elm, lit);
	      assert (tmp >= 0);
	      if (tmp)
		continue;
	      PUSH (elm->clause, lit);
	    }
	  const int size = COUNT (elm->clause);
	  Clause *r = new_clause (elm->clause.start, size);
	  if (c->dual)
	    assert (d->dual), r->dual = 1;
	  LOGCLS (r, "resolvent");
	  add_clause_to_cnf (r, elm->cnf);
	  connect_elimination_clause (elm, r);
	}
      for (int i = 0; i < c->size; i++)
	unmark (elm, c->literals[i]);
      if (resolvents > limit)
	break;
    }

  for (Clause ** p = pos->start; p != pos->top; p++)
    {
      Clause *c = *p;
      assert (!c->garbage);
      LOGCLS (c, "marking garbage literal %d", pivot);
      c->garbage = 1;
    }

  for (Clause ** p = neg->start; p != neg->top; p++)
    {
      Clause *d = *p;
      assert (!d->garbage);
      LOGCLS (d, "marking garbage literal %d", -pivot);
      d->garbage = 1;
    }

  return 1;
}

static int
eliminate_variables (Elm * elm)
{
  int eliminated = 0;
  for (const int *p = elm->schedule.start; p < elm->schedule.top; p += 2)
    eliminated += eliminate_variable (elm, *p);
  collect_garbage_clauses (elm->cnf);
  msg (2, "eliminated %d variables this round", eliminated);
  return eliminated;
}

static int
cmp (const void *p, const void *q)
{
  int *a = (int *) p, *b = (int *) q;
  int s = a[1], t = b[1];
  int res = s - t;
  return res ? res : a[0] - b[0];
}

static void
schedule_elimination (Elm * elm)
{
  CLEAR (elm->schedule);
  for (int i = 1; i <= elm->max_var; i++)
    elm->score[i] = 0;
  for (int i = -elm->max_var; i <= elm->max_var; i++)
    CLEAR (elm->occs[i]);

  Clauses *clauses = &elm->cnf->clauses;

  for (Clause ** p = clauses->start; p != clauses->top; p++)
    score_elimination_clause (elm, *p);

  for (int i = 1; i <= elm->max_var; i++)
    {
      const int score = elm->score[i];
      if (i <= elm->frozen)
	{
	  LOG ("ignoring frozen variable %d", i);
	}
      else if (!score)
	{
	  LOG ("ignoring variable %d without occurrences", i);
	}
      else if (score == INT_MAX)
	{
	  LOG ("ignoring variable %d (clause size limit exceeded)", i);
	}
      else if (score > options.elimocclim)
	{
	  LOG ("ignoring variable %d (occurrence limit exceeded)", i);
	  elm->score[i] = INT_MAX;
	}
      else
	{
	  LOG ("scheduling variable %d", i);
	  PUSH (elm->schedule, i);
	  PUSH (elm->schedule, score);
	}
    }
  const int scheduled = COUNT (elm->schedule) / 2;
  LOG ("scheduled %d non-frozen variables", scheduled);

  qsort (elm->schedule.start, scheduled, 2 * sizeof (int), cmp);
  LOG ("sorted %d scheduled variables", scheduled);

  for (Clause ** p = clauses->start; p != clauses->top; p++)
    connect_elimination_clause (elm, *p);
  LOG ("connected %d scheduled variables", scheduled);
}

void
variable_elimination (CNF * cnf, int frozen)
{
  if (!options.elim)
    return;
  assert (!cnf->redundant);
  const char *type = cnf->dual ? "dual" : "primal";
  LOG ("%s variable elimination with %d frozen variables", type, frozen);
  Elm *elm = new_elimination (cnf, frozen);
  do
    {
      if (!subsume_clauses (cnf))
	break;
      schedule_elimination (elm);
    }
  while (eliminate_variables (elm));
  if (elm->eliminated)
    msg (1, "eliminated %d %s variables %.0f%% out of %d non-frozen",
	 elm->eliminated, type,
	 percent (elm->eliminated, elm->original), elm->original);
  delete_elimination (elm);
}
