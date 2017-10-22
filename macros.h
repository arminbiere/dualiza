#include "stack.h"

#define SWAP(TYPE,A,B) { \
do { \
  TYPE TMP = (A); \
  (A) = (B); \
  (B) = (A); \
} while (0)
