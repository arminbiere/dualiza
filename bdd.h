typedef struct BDD BDD;

void init_bdds ();
void reset_bdds ();

BDD * copy_bdd (BDD *);
void delete_bdd (BDD *);

int is_false_bdd (BDD *);
int is_true_bdd (BDD *);

BDD * false_bdd ();
BDD * true_bdd ();
BDD * new_bdd (int var);
BDD * not_bdd (BDD *);

BDD * and_bdd (BDD *, BDD *);
BDD * xor_bdd (BDD *, BDD *);
BDD * or_bdd (BDD *, BDD *);
BDD * ite_bdd (BDD *, BDD *, BDD *);
BDD * xnor_bdd (BDD *, BDD *);
BDD * project_bdd (BDD *, IntStack * keep);

void print_bdd_to_file (BDD *, FILE *);
void print_bdd (BDD *);

#include "name.h"

void print_one_satisfying_cube (BDD *, Name);
void print_one_falsifying_cube (BDD *, Name);
void print_all_satisfying_cubes (BDD *, Name);

void visualize_bdd (BDD *, Name);

#include "num.h"
void count_bdd (Number res, BDD *, int max_var_idx);
