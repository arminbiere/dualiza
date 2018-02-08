typedef struct Solver Solver;

int primal_sat (Solver *);
void primal_count (Number, Solver*);
void primal_enumerate (Solver *, Name);

Solver * new_solver (CNF *, IntStack * inputs);
int deref (Solver *, int);
void delete_solver (Solver *);
