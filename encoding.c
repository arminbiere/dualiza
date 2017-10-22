#include "headers.h"

Encoding * new_encoding () {
  Encoding * res;
  NEW (res);
  return res;
}

void delete_encoding (Encoding * e) {
  RELEASE (e->stack);
  DELETE (e);
}

void encode_input (Encoding * e, Gate * g, int idx) {
  assert (idx > 0);
  assert (g->op == INPUT);
  while (COUNT (e->stack) <= idx)
    PUSH (e->stack, 0);
  assert (!e->stack.start[idx]);
  e->stack.start[idx] = g;
  LOG ("input %d gate %d encoded with literal %d",
    g->input, g->idx, idx);
}

Gate * decode_literal (Encoding * e, int idx) {
  assert (0 < idx), assert (idx < COUNT (e->stack));
  Gate * res  = e->stack.start[idx];
  assert (res);
  return res;
}

void print_dimacs_encoding_to_file (Encoding * e, FILE * file) {
  const int num_encoded = COUNT (e->stack);
  for (int i = 0; i < num_encoded; i++) {
    Gate * g = e->stack.start[i];
    if (!g) continue;
    if (g->op != INPUT) continue;
    fprintf (file, "c index %d input %d gate %d", i, g->input, g->idx);
    if (g->symbol) {
      fputs (" symbol ", file);
      const char * name = g->symbol->name;
      assert (name);
      fputs (name, file);
    }
    fputc ('\n', file);
  }
}

void print_dimacs_encoding (Encoding * e) {
  print_dimacs_encoding_to_file (e, stdout);
}
