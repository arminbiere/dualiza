#include "headers.h"

int main (int argc, char ** argv) {
  const int num_numinputs = 4;
  Circuit * c = new_circuit (num_numinputs);
  Gate * input[num_numinputs + 1];
  for (int i = 1; i <= num_numinputs; i++)
    input[i] = get_input_gate (c, i);
  Gate * f = new_and_gate (c);
  connect_gates (input[1], f);
  connect_gates (input[2], f);
  Gate * g = new_xor_gate (c);
  connect_gates (input[3], g);
  connect_gates (input[4], g);
  Gate * o = new_and_gate (c);
  connect_gates (NOT (f), o);
  connect_gates (NOT (g), o);
  connect_output (c, NOT (o));
  cone_of_influence (c);
  delete_circuit (c);
  print_statistics ();
  return 0;
}
