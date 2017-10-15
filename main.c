#include "headers.h"

static void usage () {
fputs (
"usage: dualcount [ <option> ... ] [ <file> ] [ <models> ]\n"
"\n"
"where '<option>' is one of the following flags\n"
"\n"
"  -h | --help       print this command line option summary\n"
"  --version         print version\n"
"  -v                increase verbosity\n"
#ifndef NLOG       
"  -l                enable logging\n"
#endif             
"\n"
"The default mode is to count and print the number of all models.\n"
"Alternatively the input can either just be parsed and printed with\n"
"\n"               
"  -f | --formula    print formula\n"
"  -a | --aiger      print AIGER\n"
"  -d | --dimacs     print DIMACS\n"
"  -n | --negate     negate before printing\n"
"\n"
"or checked to have a model or counter-model which are printed with\n"
"\n"
"  -s | --sat        only check to be satisfiable\n"
"  -t | --tautology  only check to be tautological\n"
"  -e | --enumerate  enumerate and print models\n"
"\n"
"Then '<option> can also be one of the following long options\n"
"which all require to use an explicit argument (default values given)\n"
"\n"
, stdout);

  usage_options ();

fputs (
"\n"
"The '<file>' argument is a path to a file of the following formats\n"
"\n"
"  <formula>         ASCII format with standard operators\n"
"  <aiger>           and-inverter graph circuit in AIGER format\n"
"  <dimacs>          CNF in DIMACS format\n"
"\n"
"If no '<file>' argument is given '<stdin>' is assumed.\n"
"Finally '<models>' is a limit on the number of models.\n"
, stdout);
}

int main (int argc, char ** argv) {
  const char * input_name = 0;
  int print_dimacs = 0;
  for (int i = 1; i < argc; i++)
    if (argv[i][0] == '-' && argv[i][1] == '-') {
      if (!parse_option (argv[i]))
	die ("invalid long option '%s'", argv[i]);
    } else if (!strcmp (argv[i], "-h") ||
               !strcmp (argv[i], "--help")) usage (), exit (0);
    else if (!strcmp (argv[i], "--version"))
      printf ("%s\n", get_version ()), exit (0);
    else if (!strcmp (argv[i], "-v")) verbosity++;
#ifndef NLOG
    else if (!strcmp (argv[i], "-l")) logging++;
#endif
    else if (argv[i][0] == '-')
      die ("invalid short option '%s'", argv[i]);
    else if (input_name)
      die ("multiple inputs '%s' and '%s'", input_name, argv[i]);
    else input_name = argv[i];
#ifndef NLOG
  if (logging) verbosity = INT_MAX;
#endif
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
  assert (!allocated);
  return 0;
}
