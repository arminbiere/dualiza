#include "headers.h"

static long
bytes_clause (const int size)
{
  return sizeof (Clause) + size * sizeof (int);
}

Clause *
new_clause (const int *literals, const int size)
{
  long bytes = bytes_clause (size);
  Clause *res = malloc (bytes);
  INC_ALLOCATED (bytes);
  if (!res)
    die ("out-of-memory allocating clause of size %d", size);
  memset (res, 0, (char *) &res->size - (char *) res);
  res->size = size;
  res->search = 2;
  for (int i = 0; i < size; i++)
    res->literals[i] = literals[i];
  return res;
}

Clause *
new_unary_clause (int unit)
{
  return new_clause (&unit, 1);
}

Clause *
new_binary_clause (int first, int second)
{
  int literals[2] = { first, second };
  return new_clause (literals, 2);
}

Clause *
new_clause_arg (int first, ...)
{
  va_list ap;
  STACK (int) literals;
  INIT (literals);
  va_start (ap, first);
  for (int lit = first; lit; lit = va_arg (ap, int))
      PUSH (literals, lit);
  va_end (ap);
  Clause *res = new_clause (literals.start, COUNT (literals));
  RELEASE (literals);
  return res;
}

void
delete_clause (Clause * c)
{
  LOGCLS (c, "delete");
  assert (c);
  long bytes = bytes_clause (c->size);
  DEC_ALLOCATED (bytes);
  free (c);
}

void
print_clause_to_file (Clause * c, FILE * file)
{
  assert (c);
  assert (file);
  for (int i = 0; i < c->size; i++)
    fprintf (file, "%d ", c->literals[i]);
  fprintf (file, "0\n");
}

void
print_clause (Clause * c)
{
  print_clause_to_file (c, stdout);
}
