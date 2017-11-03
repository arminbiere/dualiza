#ifdef TEST
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
  Reader * reader = open_new_reader ("example");
  Symbols * t = new_symbols ();
  Circuit * c = parse_circuit (reader, t);
  delete_reader (reader);
  println_circuit (c);
  CNF * f = new_cnf ();
  Encoding * e = new_encoding ();
  encode_circuit (c, f, e, 0);
  print_cnf (f);
  delete_cnf (f);
  delete_encoding (e);
  delete_circuit (c);
  delete_symbols (t);
}
#endif
#if 0
{
  Buffer * b = new_buffer ();
  enqueue_buffer (b, '1');
  enqueue_buffer (b, '2');
  enqueue_buffer (b, '3');
  printf ("%c\n", dequeue_buffer (b));
  enqueue_buffer (b, '4');
  printf ("%c\n", dequeue_buffer (b));
  enqueue_buffer (b, '5');
  enqueue_buffer (b, '6');
  printf ("%c\n", dequeue_buffer (b));
  printf ("%c\n", dequeue_buffer (b));
  enqueue_buffer (b, '7');
  enqueue_buffer (b, '8');
  while (!empty_buffer (b))
    printf ("%c\n", dequeue_buffer (b));
  delete_buffer (b);
}
#endif
#if 0
{
  Reader * r = new_reader ("<stdin>", stdin);
  Symbols * t = new_symbols ();
  Circuit * c = parse_circuit (r, t);
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
#if 0
  Number n;
  init_number (n);
  println_number (n);
  int exp[] = { 7, 13, 13, 30, 32, 32, 63, 68 };
  const int nexp = sizeof exp / sizeof *exp;
  for (int i = 0; i < nexp; i++)
    add_power_of_two_to_number (n, exp[i]),
    println_number (n);
  for (int i = nexp-1; i >= 0; i--)
    sub_power_of_two_from_number (n, exp[i]),
    println_number (n);
  for (int i = 0; i < nexp; i++)
    add_power_of_two_to_number (n, exp[i]),
    println_number (n);
  for (int i = 0; i < nexp; i++)
    sub_power_of_two_from_number (n, exp[i]),
    println_number (n);
  clear_number (n);
#endif
#if 0
  Number n;
  init_number (n);
  for (int i = 0; i < 500000; i++)
    add_power_of_two_to_number (n, i);
#if 1
  println_number (n);
#endif
  clear_number (n);
#endif
#if 0
  Number n;
  init_number (n);
  add_power_of_two_to_number (n, 0);
  multiply_number_by_power_of_two (n, 1);
  println_number (n);
  multiply_number_by_power_of_two (n, 32);
  println_number (n);
  clear_number (n);
#endif
#if 1
  init_bdds ();
  BDD * a = new_bdd (0);
  BDD * b = new_bdd (1);
  assert (a != b);
  BDD * c = new_bdd (0);
  assert (a == c);
  print_bdd (a);
  print_bdd (b);
  BDD * d = and_bdd (a, b);
  print_bdd (d);
  visualize_bdd (d);
  BDD * e = not_bdd (b);
  print_bdd (e);
  visualize_bdd (e);
  delete_bdd (a);
  delete_bdd (b);
  delete_bdd (c);
  delete_bdd (d);
  delete_bdd (e);
  reset_bdds ();
#endif
}
#endif

