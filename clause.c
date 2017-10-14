#include "headers.h"

static long bytes_clause (const int size) {
  return sizeof (Clause) + size * sizeof (int);
}

Clause * new_clause (const int * literals, const int size) {
  long bytes = bytes_clause (size);
  Clause * res = malloc (bytes);
  INC_ALLOCATED (bytes);
  if (!res) die ("out-of-memory allocating clause of size %d", size);
  memset (res, 0, (char*) &res->size - (char*) res);
  res->size = size;
  for (int i = 0; i < size; i++)
    res->literals[i] = literals[i];
  return res;
}

void delete_clause (Clause * c) {
  assert (c);
  long bytes = bytes_clause (c->size);
  DEC_ALLOCATED (bytes);
  free (c);
}

void print_clause_to_file (Clause * c, FILE * file) {
  assert (c);
  assert (file);
  for (int i = 0; i < c->size; i++)
    fprintf (file, "%d ", c->literals[i]);
  fprintf (file, "0\n");
}

void print_clause (Clause * c) { print_clause_to_file (c, stdout); }
