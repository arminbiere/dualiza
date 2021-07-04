typedef struct Clause Clause;

struct Clause
{
  long id;
  char dual, redundant, garbage, recent, active;	// flags
  int glue, size, search;
  int literals[];		// embedded literals
};

Clause *new_clause (const int *, const int size);
Clause *new_unary_clause (int);
Clause *new_binary_clause (int, int);
Clause *new_clause_arg (int, ...);
void delete_clause (Clause *);

void print_clause_to_file (Clause *, FILE *);
void print_clause (Clause *);
