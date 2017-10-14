#include "stack.h"

struct Clause;

typedef STACK (struct Clause *) Clauses;
typedef struct CNF CNF;

struct CNF {
  int positive, negative, irredundant, redundant, active;
  Clauses clauses;
};

CNF * new_cnf ();
void delete_cnf (CNF *);

int maximum_variable_index (CNF *);
void print_cnf_to_file (CNF *, FILE *);
void print_cnf (CNF *);

void add_clause_to_cnf (Clause *, CNF *);
void mark_clause_active (Clause *, CNF *);
void unmark_clause_active (Clause *, CNF *);

void collect_garbage_clause (CNF *);
