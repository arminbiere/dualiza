typedef struct Solver Solver;

Solver * new_solver (CNF * primal, IntStack * inputs, CNF * dual);
void limit_number_of_partial_models (Solver *, long limit);

int primal_sat (Solver *);
int dual_sat (Solver *);
int deref (Solver *, int);

void primal_count (Number, Solver*);
void dual_count (Number, Solver*);

void primal_enumerate (Solver *, Name);
void dual_enumerate (Solver *, Name);

void delete_solver (Solver *);
