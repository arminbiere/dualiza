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
  Gate * o = new_or_gate (c);
  connect_gates (f, o);
  connect_gates (g, o);
  connect_output (c, o);
  cone_of_influence (c);
  println_circuit (c);
  Circuit * d = negate_circuit (c);
  delete_circuit (c);
  println_circuit (d);
  delete_circuit (d);
  print_statistics ();
  return 0;
}
