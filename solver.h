typedef struct Solver Solver;

int primal_sat (Solver *);
void primal_count (Number, Solver*);
void primal_enumerate (Solver *, Name);

void dual_count (Number, Solver*);

Solver * new_solver (CNF * primal, IntStack * inputs, CNF * dual);
int deref (Solver *, int);
void delete_solver (Solver *);
