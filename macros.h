#include "stack.h"

#define SWAP(TYPE,A,B) \
do { \
  TYPE TMP = (A); \
  (A) = (B); \
  (B) = TMP; \
} while (0)

#define NOT(P) ((void*)(1l^(long)(P)))
#define SIGN(P) ((int)(1l&(long)(P)))
#define STRIP(P) ((void*)(~1l&(long)(P)))

#define MAX(A,B) ((A) < (B) ? (B) : (A))

#define ZERO(P) \
do { \
  memset ((P), 0, sizeof *(P)); \
} while (0)

#define cover(C) \
do { \
  if (!(C)) break; \
  fflush (stdout); \
  fprintf (stderr, \
    "dualiza: %s:%d: %s: Coverage goal `%s' reached\n", \
    __FILE__, __LINE__, __FUNCTION__, #C); \
  fflush (stderr); \
  abort (); \
} while (0)

#define fatal(FMT,ARGS...) \
do { \
  if (!(C)) break; \
  fflush (stdout); \
  fprintf (stderr, \
    "dualiza: %s:%d: %s: Fatal error: FMT\n", \
    __FILE__, __LINE__, __FUNCTION__, #ARGS); \
  fflush (stderr); \
  abort (); \
} while (0)
