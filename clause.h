typedef struct Clause Clause;

struct Clause {
  char negative, redundant, active, recent, garbage;
  int glue, size, literals[];
};

Clause * new_clause (const int * literals, const int size);
Clause * new_binary_clause (int first, int second);
Clause * new_clause_arg (int first, ...);
void delete_clause (Clause *);

void print_clause_to_file (Clause *, FILE *);
void print_clause (Clause *);
