typedef struct Primal Primal;

Primal * new_primal (CNF *, IntStack * inputs);
void primal_input (Primal *, int);
int primal_sat (Primal *);
void primal_count (Number, Primal*);
void primal_enumerate (Primal *, Name);
int primal_deref (Primal *, int lit);
void delete_primal (Primal *);
