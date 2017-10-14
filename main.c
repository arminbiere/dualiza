#include "headers.h"

static void usage () {
fputs (
"usage: dualcount [ <option> ... ] [ <dimacs> | <aiger> | <formula> ]\n"
"\n"
"where <option> is one of the following flags\n"
"\n"
"  -h | --help    print this command line option summary\n"
"  --version      print version\n"
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
    else if (!strcmp (argv[i], "--version"))
      printf ("%s\n", get_version ()), exit (0);
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
