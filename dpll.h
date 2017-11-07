typedef struct DPLL DPLL;

DPLL * new_dpll (CNF *, IntStack * inputs);
int dpll_sat (DPLL *);
int dpll_deref (DPLL *);
void dpll_stats (DPLL *);
void delete_dpll (DPLL *);