#include "headers.h"

CNF * new_cnf () {
  LOG ("new CNF");
  CNF * res;
  NEW (res);
  return res;
}

static void check_cnf (CNF * cnf) {
#ifndef NDEBUG
  long irredundant, redundant, active, clauses;
  irredundant = redundant = active = clauses = 0;
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++) {
    Clause * c = *p;
    if (c->redundant) redundant++; else irredundant++;
    if (c->active) active++;
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

void delete_cnf (CNF * cnf) {
  LOG ("delete CNF");
  check_cnf (cnf);
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++)
    delete_clause (*p);
  RELEASE (cnf->clauses);
  DELETE (cnf);
}

void add_clause_to_cnf (Clause * c, CNF * cnf) {
  assert (c), assert (cnf);
  if (c->redundant) cnf->redundant++; else cnf->irredundant++;
  assert (!c->active);
  c->id = cnf->added++;
  PUSH (cnf->clauses, c);
  LOG ("added clause %lu to CNF", c->id);
}

static void collect_garbage_clause (Clause * c, CNF * cnf) {
  assert (!c->active);
  assert (c->garbage);
  if (c->redundant) assert (cnf->redundant > 0), cnf->redundant--;
  else assert (cnf->irredundant > 0), cnf->irredundant--;
  delete_clause (c);
}

void mark_clause_active (Clause * c, CNF * cnf) {
  assert (!c->active);
  c->active = 1;
  cnf->active++;
}

void mark_clause_inactive (Clause * c, CNF * cnf) {
  assert (c->active);
  c->active = 0;
  assert (cnf->active > 0);
  cnf->active--;
}

void collect_garbage_clauses (CNF * cnf) {
  Clause ** p = cnf->clauses.start, ** q = p;
  long collected = 0;
  while (p < cnf->clauses.top) {
    Clause * c = *p++;
    if (!c->active && c->garbage) {
      collect_garbage_clause (c, cnf);
      collected++;
    } else *q++ = c;
  }
  cnf->clauses.top = q;
  check_cnf (cnf);
  LOG ("collected %ld garbage clauses", collected);
  stats.collected += collected;
}

int maximum_variable_index (CNF * cnf) {
  int res = 0;
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++) {
    Clause * c = *p;
    for (int i = 0; i < c->size; i++) {
      int idx = abs (c->literals[i]);
      if (idx > res) res = idx;
    }
  }
  return res;
}

void print_cnf_to_file (CNF * cnf, FILE * file) {
  int m = maximum_variable_index (cnf);
  long n = COUNT (cnf->clauses);
  fprintf (file, "p cnf %d %ld\n", m, n);
  for (Clause ** p = cnf->clauses.start; p < cnf->clauses.top; p++)
    print_clause_to_file (*p, file);
}

void print_cnf (CNF * cnf) { print_cnf_to_file (cnf, stdout); }
