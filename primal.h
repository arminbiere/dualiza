typedef struct Primal Primal;

Primal * new_primal (CNF *, IntStack * inputs);
int primal_sat (Primal *);
void primal_count (Number, Primal*);
void primal_enumerate (Primal *, Name);
int primal_deref (Primal *, int);
void delete_primal (Primal *);
