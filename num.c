#include "headers.h"

Number * new_number () {
  Number * res;
  NEW (res);
  return res;
}

void delete_number (Number * n) {
  RELEASE (n->stack);
  DELETE (n);
}

void print_number_to_file (Number * n, FILE * file) {
  const int size = COUNT (n->stack);
  int non_zero = 0;
  for (int i = size-1; i >= 0; i--) {
    unsigned m = n->stack.start[i];
    if (!m) continue;
    if (non_zero++) fputs (" + ", file);
    fprintf (file, "%u", m);
    if (i) fprintf (file, "*2^%d", 32*i);
  }
  if (!non_zero) fputc ('0', file);
}

void println_number_to_file (Number * n, FILE * file) {
  print_number_to_file (n, file);
  fputc ('\n', file);
}

void println_number (Number * n) {
  println_number_to_file (n, stdout);
}

void add_power_of_two_to_number (Number * n, int p) {
  assert (n);
  assert (p >= 0);
  long word = p >> 5;
  const int bit = p & 31;
  unsigned inc = 1u << bit;
  for (;;) {
    if (word < COUNT (n->stack)) {
      if ((n->stack.start[word] += inc)) break;
      inc = 1, word++;
    } else PUSH (n->stack, 0);
  }
}
