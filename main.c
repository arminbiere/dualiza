#include "headers.h"

static void usage () {
fputs (
"usage: dualcount [ <option> ... ] [ <file> ] [ <limit> ]\n"
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
"Then '<option>' can also be one of the following long options\n"
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
"Finally '<limit>' is a limit on the number of partial models.\n"
, stdout);
}

static int formula, aiger, dimacs, negate;
static int sat, tautology, enumerate, limited;
static long limit;
 
static void check_options () {
  const int printing = (formula != 0) + (aiger != 0) + (dimacs != 0);
  const int checking = (sat != 0) + (tautology != 0);
#define FORMULA   (formula  >0?" '--formula'"  :(formula<0  ?" '-f'":""))
#define AIGER     (aiger    >0?" '--aiger'"    :(aiger<0    ?" '-a'":""))
#define DIMACS    (dimacs   >0?" '--dimacs'"   :(dimacs<0   ?" '-d'":""))
#define SAT       (sat      >0?" '--sat'"      :(sat<0      ?" '-s'":""))
#define TAUTOLOGY (tautology>0?" '--tautology'":(tautology<0?" '-t'":""))
#define ENUMERATE (enumerate>0?" '--enumerate'":(enumerate<0?" '-e'":""))
#define NEGATE    (negate   >0?" '--negate'"   :(negate<0   ?" '-n'":""))
#define PRINTING FORMULA,AIGER,DIMACS
#define CHECKING SAT,TAUTOLOGY
  if (printing > 1)
    die ("too many printing options:%s%s%s", PRINTING);
  if (checking > 1)
    die ("too many checking options:%s%s", CHECKING);
  if (printing && checking)
    die ("can not combine%s%s%s and%s%s", PRINTING, CHECKING);
  if (printing && enumerate)
    die ("can not combine%s%s%s and%s", PRINTING, ENUMERATE);
  if (checking && enumerate)
    die ("can not combine%s%s and%s", CHECKING, ENUMERATE);
  if (checking && negate)
    die ("can not combine%s%s and%s", CHECKING, NEGATE);
  if (!printing && negate)
    die ("can not use%s without printing option", NEGATE);
  assert (!(enumerate && negate));
#undef FORMULA
#undef AIGER
#undef DIMACS
#undef SAT
#undef TAUTOLOGY
#undef ENUMERATE
#undef NEGATE
#undef PRINTING
#undef CHECKING
}

static int is_non_negative_number_string (const char * s) {
  const char * p = s;
  char ch = *p++;
  if (!isdigit (ch)) return 0;
  while ((ch = *p++))
    if (!isdigit (ch)) return 0;
  return 1;
}

static long parse_non_negative_number (const char * s) {
  long res = 0;
  if (s[0] == '0' && s[1])
    die ("non-zero number '%s' starts with '0'", s);
  for (const char * p = s; *p; p++) {
    if (LONG_MAX/10 < res) die ("number '%s' really too large", s);
    res *= 10;
    const int digit = *p - '0';
    if (LONG_MAX - digit < res) die ("number '%s' too large", s);
    res += digit;
  }
  return res;
}

int main (int argc, char ** argv) {
  const char * input_name = 0;
  for (int i = 1; i < argc; i++)
    if (!strcmp (argv[i], "-h") ||
        !strcmp (argv[i], "--help")) usage (), exit (0);
    else if (!strcmp (argv[i], "--version"))
      printf ("%s\n", get_version ()), exit (0);
    else if (!strcmp (argv[i], "-v")) verbosity++;
#ifndef NLOG
    else if (!strcmp (argv[i], "-l")) logging++;
#endif
    else if (!strcmp (argv[i], "-f"))            formula = -1;
    else if (!strcmp (argv[i], "-a"))              aiger = -1;
    else if (!strcmp (argv[i], "-d"))             dimacs = -1;
    else if (!strcmp (argv[i], "-n"))             negate = -1;
    else if (!strcmp (argv[i], "-s"))                sat = -1;
    else if (!strcmp (argv[i], "-t"))          tautology = -1;
    else if (!strcmp (argv[i], "-e"))          enumerate = -1;
    else if (!strcmp (argv[i], "--formula"))     formula = +1;
    else if (!strcmp (argv[i], "--aiger"))         aiger = +1;
    else if (!strcmp (argv[i], "--dimacs"))       dimacs = +1;
    else if (!strcmp (argv[i], "--negate"))       negate = +1;
    else if (!strcmp (argv[i], "--sat"))             sat = +1;
    else if (!strcmp (argv[i], "--tautology")) tautology = +1;
    else if (!strcmp (argv[i], "--enumerate")) enumerate = +1;
    else if (argv[i][0] == '-' && argv[i][1] == '-') {
      if (!parse_option (argv[i]))
	die ("invalid long option '%s'", argv[i]);
    } else if (argv[i][0] == '-')
      die ("invalid short option '%s'", argv[i]);
    else if (is_non_negative_number_string (argv[i])) {
      if (limited) die ("multiple limits '%ld' and '%s'", limit, argv[i]);
      limit = parse_non_negative_number (argv[i]);
      limited = 1;
    } else if (input_name)
      die ("multiple files '%s' and '%s'", input_name, argv[i]);
    else input_name = argv[i];
#ifndef NLOG
  if (logging) verbosity = INT_MAX;
#endif
  check_options ();
  msg (1, "DualCount #SAT Solver");
  msg (1, "Copyright (C) 2017 Armin Biere Johannes Kepler University Linz");
  print_version ();
  print_options ();
  Input * input;
  if (input_name) input = open_new_input (input_name);
  else input = new_input_from_stdin ();
  msg (1, "reading from '%s'", input->name);
  delete_input (input);
  test ();
  print_statistics ();
  assert (!allocated);
  return 0;
}
