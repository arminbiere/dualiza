typedef struct Solver Solver;

int primal_sat (Solver *);
void primal_count (Number, Solver*);
void primal_enumerate (Solver *, Name);

int dual_sat (Solver *);
void dual_count (Number, Solver*);
void dual_enumerate (Solver *, Name);

Solver * new_solver (CNF * primal, IntStack * inputs, CNF * dual);
int deref (Solver *, int);
void delete_solver (Solver *);
