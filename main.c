#include "headers.h"

int main (int argc, char ** argv) {
  Circuit * c = new_circuit (4);
  Gate * a = new_and_gate (c);
  Gate * x = get_input_gate (c, 1);
  Gate * y = get_input_gate (c, 2);
  connect_gates (x, a);
  connect_gates (y, a);
  connect_output (c, a);
  cone_of_influence (c);
  delete_circuit (c);
  print_statistics ();
  return 0;
}
