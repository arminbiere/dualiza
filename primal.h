typedef struct Primal Primal;

Primal * new_primal (CNF *, IntStack * inputs);
int primal_sat (Primal *);
int primal_deref (Primal *, int lit);
void primal_stats (Primal *);
void delete_primal (Primal *);
