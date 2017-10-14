#include "headers.h"

static void print_gate_to_file (Gate * g, Operator outer, FILE * file) {
  const int sign = SIGN (g);
  if (sign) g = NOT (g);
  if (g->op == FALSE) fprintf (file, "%c", sign ? '1' : '0');
  else {
    if (sign) fprintf (file, "!");
    if (g->op == INPUT) fprintf (file, "x%d", g->input);
    else {
      int parenthesis;
      if (sign) parenthesis = 1;
      else if (g->op < ITE) parenthesis = (g->op > outer);
      else parenthesis = (g->op >= outer);
      if (parenthesis) fprintf (file, "(");
      if (g->op == ITE) {
	Gate ** inputs = g->inputs.start;
	assert (COUNT (g->inputs) == 3);
	print_gate_to_file (inputs[0], ITE, file);
	fprintf (file, " ? ");
	print_gate_to_file (inputs[1], ITE, file);
	fprintf (file, " : ");
	print_gate_to_file (inputs[2], ITE, file);
      } else {
	char op = '=';
	if (g->op == AND) op = '&';
	else if (g->op == OR) op = '|';
	else if (g->op == XOR) op = '^';
	else assert (g->op == XNOR);
	for (Gate ** p = g->inputs.start; p < g->inputs.top; p++) {
	  if (p != g->inputs.start) fprintf (file, " %c ", op);
	  print_gate_to_file (*p, g->op, file);
	}
      }
      if (parenthesis) fprintf (file, ")");
    }
  }
}

void println_gate (Gate * g) {
  print_gate_to_file (g, -1, stdout);
  fputc ('\n', stdout);
}

void print_circuit_to_file (Circuit * c, FILE * file) {
  check_circuit_connected (c);
  print_gate_to_file (c->output, -1, file);
}

void println_circuit (Circuit * c) {
  print_circuit_to_file (c, stdout);
  fputc ('\n', stdout);
}

