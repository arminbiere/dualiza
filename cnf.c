#include "headers.h"

#ifndef NLOG

static const char *
cnf_type (CNF * cnf)
{
  return cnf->dual ? "dual" : "primal";
}

#endif

CNF *
new_cnf (int dual)
{
  CNF *res;
  NEW (res);
  res->dual = dual;
  LOG ("new %s CNF", cnf_type (res));
  return res;
}

static void
check_cnf (CNF * cnf)
{
#ifndef NDEBUG
  long irredundant, redundant, active, clauses;
  irredundant = redundant = active = clauses = 0;
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++)
    {
      Clause *c = *p;
      if (c->redundant)
	redundant++;
      else
	irredundant++;
      if (c->active)
	active++;
      clauses++;
    }
  assert (COUNT (cnf->clauses) == clauses);
  assert (cnf->irredundant == irredundant);
  assert (cnf->redundant == redundant);
  assert (cnf->active == active);
  assert (irredundant + redundant == clauses);
  assert (active <= clauses);
#endif
}

void
delete_cnf (CNF * cnf)
{

  LOG ("delete %s CNF", cnf_type (cnf));
  check_cnf (cnf);
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++)
    delete_clause (*p);
  RELEASE (cnf->clauses);
  DELETE (cnf);
}

void
add_clause_to_cnf (Clause * c, CNF * cnf)
{
  assert (c), assert (cnf);
  assert (c->dual == cnf->dual);
  if (c->redundant)
    cnf->redundant++;
  else
    cnf->irredundant++;
  assert (!c->active);
  c->id = cnf->added++;
  PUSH (cnf->clauses, c);
  LOG ("added clause %lu to %s CNF", c->id, cnf_type (cnf));
}

static void
collect_garbage_clause (Clause * c, CNF * cnf)
{
  assert (!c->active);
  assert (c->garbage);
  if (c->redundant)
    assert (cnf->redundant > 0), cnf->redundant--;
  else
    assert (cnf->irredundant > 0), cnf->irredundant--;
  delete_clause (c);
}

void
mark_clause_active (Clause * c, CNF * cnf)
{
  assert (c), assert (cnf);
  assert (c->dual == cnf->dual);
  assert (!c->active);
  c->active = 1;
  cnf->active++;
}

void
mark_clause_inactive (Clause * c, CNF * cnf)
{
  assert (c), assert (cnf);
  assert (c->dual == cnf->dual);
  assert (c->active);
  c->active = 0;
  assert (cnf->active > 0);
  cnf->active--;
}

void
collect_garbage_clauses (CNF * cnf)
{
  Clause **p = cnf->clauses.start, **q = p;
  long collected = 0;
  while (p < cnf->clauses.top)
    {
      Clause *c = *p++;
      if (!c->active && c->garbage)
	{
	  collect_garbage_clause (c, cnf);
	  collected++;
	}
      else
	*q++ = c;
    }
  cnf->clauses.top = q;
  check_cnf (cnf);
  LOG ("collected %ld garbage clauses in %s CNF", collected, cnf_type (cnf));
  stats.collected += collected;
}

int
maximum_variable_index (CNF * cnf)
{
  int res = 0;
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++)
    {
      Clause *c = *p;
      for (int i = 0; i < c->size; i++)
	{
	  int idx = abs (c->literals[i]);
	  if (idx > res)
	    res = idx;
	}
    }
  return res;
}

int
minimum_variable_index_above (CNF * cnf, int lower_limit)
{
  int res = INT_MAX;
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++)
    {
      Clause *c = *p;
      for (int i = 0; i < c->size; i++)
	{
	  int idx = abs (c->literals[i]);
	  if (idx > lower_limit && idx < res)
	    res = idx;
	}
    }
  return res;
}

void
print_cnf_to_file (CNF * cnf, int max_relevant, FILE * file)
{
  int max_var = maximum_variable_index (cnf);
  int m;
  if (max_var > max_relevant)
    {
      LOG ("maximum variable in CNF '%d' larger than maximum relevant '%d'",
	   max_var, max_relevant);
      m = max_var;
    }
  else if (max_var < max_relevant)
    {
      LOG ("maximum variable in CNF '%d' smaller than maximum relevant '%d'",
	   max_var, max_relevant);
      m = max_relevant;
    }
  else
    m = max_relevant;
  long n = COUNT (cnf->clauses);
  fprintf (file, "p cnf %d %ld\n", m, n);
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++)
    print_clause_to_file (*p, file);
}

void
print_cnf (CNF * cnf)
{
  print_cnf_to_file (cnf, maximum_variable_index (cnf), stdout);
}
