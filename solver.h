typedef struct Solver Solver;

int primal_sat (Solver *);
void primal_count (Number, Solver*);
void primal_enumerate (Solver *, Name);

void enable_model_printing (Solver *, Name);
void get_number_of_models (Number, Solver *);
void limit_number_of_partial_models (Solver *, long limit);

int dual_sat (Solver *);
void dual_count (Number, Solver*);
void dual_enumerate (Solver *, Name);

Solver * new_solver (CNF * primal, IntStack * inputs, CNF * dual);
int deref (Solver *, int);
void delete_solver (Solver *);
