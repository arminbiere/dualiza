typedef struct Clause Clause;

struct Clause {
  char negative, learned, active, recent;
  int glue, size, literals[];
};

Clause * new_clause (const int * literals, const int size);
void delete_clause (Clause *);
