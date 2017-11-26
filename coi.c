#include "headers.h"

static void coi (Circuit * c, Gate * g, int sign) {
  if (SIGN (g)) sign = !sign, g = NOT (g);
  assert (c);
  assert (g);
  const int mark = (1<<sign);
  if (g->mark & mark) return;
  g->mark |= mark;
  if (sign) g->neg = 1; else g->pos = 1;
  switch (g->op) {
    case OR:
    case AND:
      for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
	coi (c, *p, sign);
      break;
    case ITE:
      for (Gate ** p = g->inputs.start; p < g->inputs.top; p++) {
	coi (c, *p, sign);
	if (p == g->inputs.start) coi (c, *p, !sign);
      }
      break;
    case XOR:
    case XNOR:
      for (Gate ** p = g->inputs.start; p < g->inputs.top; p++)
	coi (c, *p, sign), coi (c, *p, !sign);
      break;
    case FALSE:
    case INPUT:
      break;
  }
}

void cone_of_influence (Circuit * c) {
  check_circuit_connected (c);
  for (Gate ** p = c->gates.start; p < c->gates.top; p++) {
    Gate * g = *p;
    g->pos = g->neg = g->mark = 0;
  }
  coi (c, c->output, 0);
  int pos = 0, neg = 0, both = 0, disconnected = 0;
  for (Gate ** p = c->gates.start; p < c->gates.top; p++) {
    Gate * g = *p;
    assert (!SIGN (g));
    if (g->pos && g->neg) both++;
    else if (g->pos) pos++;
    else if (g->neg) neg++;
    else disconnected++;
    g->mark = 0;
  }
  msg (2, "cone of influence: %d pos, %d neg, %d both, %d disconnected",
    pos, neg, both, disconnected);
}

