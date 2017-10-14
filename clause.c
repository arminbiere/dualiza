#include "headers.h"

static long bytes_clause (const int size) {
  return sizeof (Clause) + size * sizeof (int);
}

Clause * new_clause (const int * literals, const int size) {
  long bytes = bytes_clause (size);
  Clause * res = malloc (bytes);
  INC_ALLOCATED (bytes);
  if (!res) die ("out-of-memory allocating %ld clause bytes", bytes);
  memset (res, 0, (char*) &res->size - (char*) res);
  res->size = size;
  for (int i = 0; i < size; i++)
    res->literals[i] = literals[i];
  return res;
}

void delete_clause (Clause * c) {
  long bytes = bytes_clause (c->size);
  DEC_ALLOCATED (bytes);
  free (c);
}
