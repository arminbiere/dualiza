typedef struct BDD BDD;

void init_bdds ();
void reset_bdds ();

BDD * copy_bdd (BDD *);
void delete_bdd (BDD *);

BDD * false_bdd ();
BDD * true_bdd ();
BDD * new_bdd (int var);
BDD * not_bdd (BDD *);

BDD * and_bdd (BDD *, BDD *);
BDD * xor_bdd (BDD *, BDD *);
BDD * or_bdd (BDD *, BDD *);
BDD * ite_bdd (BDD *, BDD *, BDD *);
BDD * xnor_bdd (BDD *, BDD *, BDD *);

extern long bdd_lookups, bdd_collisions;
extern long cache_lookups, cache_collisions;
