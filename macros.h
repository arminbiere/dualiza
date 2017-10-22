#include "stack.h"

#define SWAP(TYPE,A,B) \
do { \
  TYPE TMP = (A); \
  (A) = (B); \
  (B) = TMP; \
} while (0)
