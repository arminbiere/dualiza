#include "headers.h"

int main (int argc, char ** argv) {
  verbosity = 1;
  const int num_inputs = 5;
  Circuit * c = new_circuit (num_inputs);
  Gate * input[num_inputs + 1];
  for (int i = 1; i <= num_inputs; i++)
    input[i] = get_input_gate (c, i);
  Gate * f = new_and_gate (c);
  connect_gates (input[1], f);
  connect_gates (NOT (input[2]), f);
  connect_gates (input[3], f);
  Gate * g = new_xor_gate (c);
  connect_gates (input[4], g);
  connect_gates (NOT (input[5]), g);
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
