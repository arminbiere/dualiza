typedef struct Stats Stats;

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

extern Stats stats;

void print_statistics ();
