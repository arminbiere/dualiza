#include "headers.h"

static Gate * new_gate (Circuit * c) {
  assert (COUNT (c->gates) < INT_MAX);
  Gate * res;
  NEW (res);
  res->idx = COUNT (c->gates);
  PUSH (c->gates, res);
  return res;
}

static Gate * new_input (Circuit * c) {
  Gate * res = new_gate (c);
  res->op = INPUT;
  assert (res->idx < c->num_inputs);
  return res;
}

Circuit * new_circuit (int num_inputs) {
  Circuit * res;
  NEW (res);
  res->num_inputs = num_inputs;
  for (int i = 0; i < num_inputs; i++)
    new_input (res);
  msg ("new circuit with %d inputs", num_inputs);
  return res;
}

static void delete_gate (Gate * g) {
  DELETE (g);
}

void delete_circuit (Circuit * c) {
  msg ("deleting circuit with %ld gates", COUNT (c->gates));
  for (Gate ** p = c->gates.start; p != c->gates.top; p++)
    delete_gate (*p);
  RELEASE (c->gates);
  DELETE (c);
}
