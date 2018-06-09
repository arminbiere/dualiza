typedef struct Stats Stats;
typedef struct Rules Rules;

#define RULES \
\
RULE (EP0) \
RULE (EPN) \
\
RULE (BP0) \
RULE (BN0F) \
RULE (BN0L) \
\
RULE (AP0) \
RULE (AP0F) \
RULE (AP0L) \
\
RULE (JP0) \
RULE (JN0F) \
RULE (JN0L) \
\
RULE (DX) \
RULE (DYS) \
\
RULE (UP) \
RULE (UNXY) \
RULE (UNT) \
\
RULE (FP) \
RULE (FN)

struct Rules {
# define RULE(NAME) \
  long NAME;
  RULES
#undef RULE
};

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
