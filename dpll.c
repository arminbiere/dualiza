#include "headers.h"

typedef struct Var Var;
typedef STACK (Clause*) Occ;

struct Var {
  char input;
  signed char val;
  Var * prev, * next;
  Occ occs[2];
};

struct DPLL {
  int max_var;
  int num_inputs;
  Var * vars;
  CNF * cnf;
};

static Var * var (DPLL * dpll, int lit) {
  assert (lit);
  int idx = abs (lit);
  assert (idx <= dpll->max_var);
  return dpll->vars + idx;
}

DPLL * new_dpll (CNF * cnf, IntStack * inputs) {
  LOG ("new DPLL over %ld clauses", COUNT (cnf->clauses));
  DPLL * res;
  NEW (res);
  int max_var_in_cnf = maximum_variable_index (cnf);
  LOG ("maximum variable index %d in CNF", max_var_in_cnf);
  int max_var_in_inputs = 0;
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (0 < input);
    if (input > max_var_in_inputs) max_var_in_inputs = input;
  }
  LOG ("maximum input variable index %d", max_var_in_inputs);
  res->max_var = MAX (max_var_in_inputs, max_var_in_inputs);
  ALLOC (res->vars, res->max_var + 1);
  for (const int * p = inputs->start; p < inputs->top; p++) {
    int input = abs (*p);
    assert (input < res->max_var);
    Var * v = var (res, input);
    if (v->input) continue;
    LOG ("input variable %d", input);
    res->num_inputs++;
    v->input = 1;
  }
  return res;
}

int dpll_sat (DPLL * dpll) {
  return 0;
}

int dpll_deref (DPLL * dpll) {
  return 0;
}

void dpll_stats (DPLL * dpll) {
}

void delete_dpll (DPLL * dpll) {
}
