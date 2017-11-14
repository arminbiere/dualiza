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
