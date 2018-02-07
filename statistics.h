typedef struct Stats Stats;

struct Stats {
  long decisions, propagated, conflicts;
  struct { long tracked, jumped, forced; } back;
  long bumped, searched, learned;
  struct { long clauses, literals; } blocked;
  long reductions, collected, rephased;
  long reports, restarts, reused;
  struct {
    struct { long lookups, collisions; } node, cache;
  } bdd;
  struct {
    long lookups, collisions;
  } symbol;
  struct { long max, current; } bytes;
};

extern Stats stats;

void print_statistics ();
