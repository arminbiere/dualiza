#include "headers.h"

static int is_dimacs_file (Reader * reader) {
  return 0;
}

Type get_file_type (Reader * reader) {
  assert (reader);
  msg (2, "trying to determine file type of input file '%s'", reader->name);
  Type res = NOTYPE;
  return res;
}
