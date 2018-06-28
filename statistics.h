typedef struct Stats Stats;
typedef struct Rules Rules;

/*------------------------------------------------------------------------*/

struct Rules {
  long EP1,  EP0,  EN0;
  long BP0F, JP0;
  long BP1F, BP1L;
  long BN0F, BN0L;
#if 0
  long JN0F, JN0L;
#endif
  long DX,   DY,   DS;
  long UP,   UNX,  UNY,  UNT;
  long FP,   FN;
};

/*------------------------------------------------------------------------*/

struct Stats {
  long decisions, flipped;
  long reductions, collected;
  long reports, restarts, reused;
  long pivots, resolutions, eliminated;
  struct { struct { long shared; } dual; } units;
  struct { long dual, primal; } conflicts, propagated;
  struct { long clauses, subsumed, literals; } blocked;
  struct { long tracked, jumped, forced, discounting; } back;
  long bumped, searched, learned, subsumed, strengthened, tried;
  struct { struct { long lookups, collisions; } node, cache; } bdd;
  struct { long lookups, collisions; } symbol;
  struct { long counted, discounted; } models;
  struct { long max, current; } bytes;
};

/*------------------------------------------------------------------------*/

extern Stats stats;
extern Rules rules;

/*------------------------------------------------------------------------*/

void print_rules ();
void print_statistics ();
