typedef struct Stats Stats;

struct Stats {
  long decisions, propagated, conflicts;
  long backtracked, backjumped;
  long bumped, searched, learned;
  long reductions;
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
