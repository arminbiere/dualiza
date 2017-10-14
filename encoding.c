#include "headers.h"

Encoding * new_encoding () {
  Encoding * res;
  NEW (res);
  return res;
}

void delete_encoding (Encoding * e) {
  RELEASE (*e);
  DELETE (e);
}

void encode_gate (Encoding * e, Gate * g, int idx) {
  assert (idx > 0);
  assert (g->op == INPUT);
  while (COUNT (*e) <= idx)
    PUSH (*e, 0);
  assert (!e->start[idx]);
  e->start[idx] = g;
}

Gate * decode_literal (Encoding * e, int idx) {
  assert (0 < idx), assert (idx < COUNT (*e));
  Gate * res  = e->start[idx];
  assert (res);
  return res;
}
