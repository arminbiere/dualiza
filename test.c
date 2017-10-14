#include "headers.h"

void test () {
#if 0
{
  const int num_inputs = 5;
  Circuit * c = new_circuit (num_inputs);
  Gate * input[num_inputs + 1];
  for (int i = 1; i <= num_inputs; i++)
    input[i] = new_input_gate (c, i);
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
}
#endif
#if 0
{
  int literals[3] = { -1, 3, -2 };
  printf ("sizeof (Clause) = %ld\n", sizeof (Clause));
  Clause * c = new_clause (literals, 3);
  print_clause (c);
  delete_clause (c);
}
#endif
#if 0
{
  CNF * cnf = new_cnf ();
  add_clause_to_cnf (new_clause_arg (0), cnf);
  add_clause_to_cnf (new_clause_arg (-1, 0), cnf);
  add_clause_to_cnf (new_binary_clause (1, 2), cnf);
  add_clause_to_cnf (new_clause_arg (1, 3, 0), cnf);
  add_clause_to_cnf (new_clause_arg (-1, -2, -3, 0), cnf);
  print_cnf (cnf);
  delete_cnf (cnf);
}
#endif
#if 0
{
  FILE * file = fopen ("example", "r");
  Reader * reader = new_reader ("example", file);
  Circuit * circuit = parse_formula (reader);
  delete_reader (reader);
  fclose (file);
  println_circuit (circuit);
  CNF * cnf = new_cnf ();
  tseitin_encoding (circuit, cnf, 0);
  delete_circuit (circuit);
  print_cnf (cnf);
  delete_cnf (cnf);
}
#endif
#if 1
{
  Reader * r = new_reader ("<stdin>", stdin);
  Symbols * t = new_symbols ();
  Circuit * c = parse_formula (r, t);
  delete_reader (r);
  println_circuit (c);
  Circuit * d = negate_circuit (c);
  delete_circuit (c);
  println_circuit (d);
  delete_circuit (d);
  delete_symbols (t);
}
#endif
#if 0
{
  Symbols * t = new_symbols ();
  Symbol * a = find_or_create_symbol (t, "a");
  Symbol * b = find_or_create_symbol (t, "b");
  Symbol * again = find_or_create_symbol (t, "a");
  Symbol * bgain = find_or_create_symbol (t, "b");
  assert (a == again);
  assert (b == bgain);
  delete_symbols (t);
}
#endif
}

