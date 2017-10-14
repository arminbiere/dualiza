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

void encode_gate (Encoding * e, Gate * g, int idx) {
  assert (idx > 0);
  assert (g->op == INPUT);
  while (COUNT (e->stack) <= idx)
    PUSH (e->stack, 0);
  assert (!e->stack.start[idx]);
  e->stack.start[idx] = g;
}

Gate * decode_literal (Encoding * e, int idx) {
  assert (0 < idx), assert (idx < COUNT (e->stack));
  Gate * res  = e->stack.start[idx];
  assert (res);
  return res;
}
