#include "headers.h"

int main (int argc, char ** argv) {
  Circuit * c = new_circuit (4);
  delete_circuit (c);
  print_statistics ();
  return 0;
}
