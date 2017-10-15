#include "headers.h"

typedef struct Aiger Aiger;

struct Aiger {
  Reader * reader;
  Symbols * symbols;
  unsigned max_index;
  unsigned num_inputs;
  unsigned num_ands;
  Gate * gates;
};

Circuit * parse_aiger (Reader * r, Symbols * t) {
  return 0;
}
