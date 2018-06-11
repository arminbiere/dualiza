typedef struct Stats Stats;
typedef struct Rules Rules;

/*------------------------------------------------------------------------*/

#define RULE(NAME) \
do { \
  rules.NAME++; \
  SOG ("RULE %s %ld", #NAME, rules.NAME); \
} while (0)

struct Rules {
  long EP1,  EP0,  EN0;
  long BP0,  BP0F, BP0L;
  long BP1F, BP1L;
  long BN0F, BN0L;
  // long AP0,  AP0F, AP0L;
  long JP0,  JN0F, JN0L;
  long DX,   DY,   DS;
  long UP,   UNX,  UNY, UNT;
  long FP,   FN;
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
