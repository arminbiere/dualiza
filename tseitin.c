#include "headers.h"

void tseitin_encoding (Circuit * circuit, CNF * cnf, int negative) {
  check_circuit_connected (circuit);
  cone_of_influence (circuit);
  int num_inputs = COUNT (circuit->inputs);
}
