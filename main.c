#include "headers.h"

void test () {
#if 0
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
#endif
#if 1
  Circuit * c = parse_formula ("<stdin>", stdin);
  println_circuit (c);
  Circuit * d = negate_circuit (c);
  delete_circuit (c);
  println_circuit (d);
  delete_circuit (d);
#endif
}

static void usage () {
fputs (
"usage: dualcount [ <option> ... ] [ <dimacs> | <aiger> | <formula> ]\n"
"\n"
"where <option> is one of the following flags\n"
"\n"
"  -h | --help    print this command line option summary\n"
"  -v             increase verbosity\n"
"\n"
"or one of the following long options with default values\n"
"\n"
, stdout);
  usage_options ();
}

int main (int argc, char ** argv) {
  const char * input_name = 0;
  for (int i = 1; i < argc; i++)
    if (argv[i][0] == '-' && argv[i][1] == '-') {
      if (!parse_option (argv[i]))
	die ("invalid long option '%s'", argv[i]);
    } else if (!strcmp (argv[i], "-h") ||
               !strcmp (argv[i], "--help")) usage (), exit (0);
    else if (!strcmp (argv[i], "-v")) verbosity = 1;
    else if (argv[i][0] == '-')
      die ("invalid short option '%s'", argv[i]);
    else if (input_name)
      die ("multiple inputs '%s' and '%s'", input_name, argv[i]);
    else input_name = argv[i];
  int close_input = 0;
  FILE * input_file;
  if (!input_name) input_name = "<stdin>", input_file = stdin;
  else if (!(input_file = fopen (input_name, "r")))
    die ("can not open '%s' for reading", input_name); 
  else close_input = 1;
  msg (1, "DualCount #SAT Solver");
  msg (1, "Copyright (C) 2017 Armin Biere Johannes Kepler University Linz");
  print_version ();
  print_options ();
  msg (1, "reading from '%s'", input_name);
  if (close_input) fclose (input_file);
  test ();
  print_statistics ();
  return 0;
}
