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
  fflush (stdout); \
  fprintf (stderr, \
    "dualiza: %s:%d: %s: Fatal error: ", \
    __FILE__, __LINE__, __FUNCTION__); \
  fprintf (stderr, FMT, ##ARGS); \
  fputc ('\n', stderr); \
  fflush (stderr); \
  abort (); \
} while (0)

#ifdef __MINGW32__
#define PRz "Iu"
#else
#define PRz "zu"
#endif
