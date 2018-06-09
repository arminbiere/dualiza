typedef struct Stats Stats;
typedef struct Rules Rules;

/*------------------------------------------------------------------------*/

struct Rules {
  long EP0, EPN;
  long BP0, BN0F, BN0L;
  long AP0, AP0F, AP0L;
  long JP0, JN0F, JN0L;
  long DX, DYS;
  long UP, UNXY, UNT;
  long FP, FN;
};

/*------------------------------------------------------------------------*/

struct Stats {
  long decisions, flipped;
  long dual_shared_units, dual_non_shared_units;
  struct { long dual, primal; } conflicts, propagated;
  struct { long tracked, jumped, forced, discounting; } back;
  long bumped, searched, learned, subsumed;
  struct { long clauses, literals; } blocked;
  long reductions, collected;
  long reports, restarts, reused;
  struct {
    struct { long lookups, collisions; } node, cache;
  } bdd;
  struct {
    long lookups, collisions;
  } symbol;
  struct { long max, current; } bytes;
  struct { long counted, discounted; } models;
};

/*------------------------------------------------------------------------*/

extern Stats stats;
extern Rules rules;

/*------------------------------------------------------------------------*/

void print_statistics ();
