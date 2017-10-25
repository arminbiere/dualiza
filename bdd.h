typedef struct Bdd Bdd;

void init_bdds ();
void reset_bdds ();

void delete_bdd (Bdd *);

Bdd * false_bdd (Bdd *);
Bdd * true_bdd (Bdd *);
Bdd * new_bdd_var (int var);
Bdd * not_bdd (Bdd *);

Bdd * and_bdd (Bdd *, Bdd *);
Bdd * xor_bdd (Bdd *, Bdd *);
Bdd * or_bdd (Bdd *, Bdd *);
Bdd * ite_bdd (Bdd *, Bdd *, Bdd *);
Bdd * xnor_bdd (Bdd *, Bdd *, Bdd *);
