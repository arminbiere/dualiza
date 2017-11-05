#include "headers.h"

#ifdef TEST
#define static /**/
#endif

static void usage () {
fputs (
"usage: dualiza [ <option> ... ] [ <file> ] [ <limit> ]\n"
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
"The '<file>' argument is a path to a file of the following formats\n"
"\n"
"  <formula>         ASCII format with standard operators\n"
"  <aiger>           and-inverter graph circuit in AIGER format\n"
"  <dimacs>          CNF in DIMACS format\n"
"\n"
"If no '<file>' argument is given '<stdin>' is assumed.  An input\n"
"file which has a '.bz2', '.gz', '.xz' or '.7z' extension is assumed\n"
"to be compressed and is decompressed during parsing on-the-fly.\n"
"The parsed input is negated after parsing with the following option\n"
"\n"
"  -n | --negate     negate before printing\n"
"\n"
"The default mode is to count and print the number of all models.\n"
"Alternatively the input can either just be parsed and printed with\n"
"\n"               
"  -f | --formula    print formula\n"
"  -a | --aiger      print AIGER\n"
"  -d | --dimacs     print DIMACS\n"
"\n"
"to '<stdout>' or optionally to a (compressed) file specified with\n"
"\n"
"  -o <file>\n"
"\n"
"or checked to have a model or counter-model which are printed with\n"
"\n"
"  -s | --sat        only check to be satisfiable\n"
"  -t | --tautology  only check to be tautological\n"
"  -e | --enumerate  enumerate and print models\n"
"\n"
"By default the SAT engine is used, or alternatively the BDD engine.\n"
"\n"
"  -b | --bdd        use BDD engine instead of SAT engine\n"
"\n"
"Then '<option>' can also be one of the following long options\n"
"which all require to use an explicit argument (default values given)\n"
"\n"
, stdout);
  usage_options ();
fputs (
"\n"
"Finally '<limit>' is a limit on the number of partial models.\n"
, stdout);
}

static int formula, aiger, dimacs, negate;
static int sat, tautology, enumerate, limited;
static int printing, checking, bdd;
static long limit;
 
static void check_options (const char * output_name) {
  printing = (formula != 0) + (aiger != 0) + (dimacs != 0);
  checking = (sat != 0) + (tautology != 0);
# define FORMULA   (formula  >0?" '--formula'"  :(formula<0  ?" '-f'":""))
# define AIGER     (aiger    >0?" '--aiger'"    :(aiger<0    ?" '-a'":""))
# define DIMACS    (dimacs   >0?" '--dimacs'"   :(dimacs<0   ?" '-d'":""))
# define SAT       (sat      >0?" '--sat'"      :(sat<0      ?" '-s'":""))
# define TAUTOLOGY (tautology>0?" '--tautology'":(tautology<0?" '-t'":""))
# define ENUMERATE (enumerate>0?" '--enumerate'":(enumerate<0?" '-e'":""))
# define BDD       (bdd      >0?" '--bdd'"      :(bdd<0      ?" '-b'":""))
# define NEGATE    (negate   >0?" '--negate'"   :(negate<0   ?" '-n'":""))
# define PRINTING FORMULA,AIGER,DIMACS
# define CHECKING SAT,TAUTOLOGY
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
  if (!printing && output_name)
    die ("can output specified without printing option");
  if (printing && bdd)
    die ("can not use%s%s%s and%s", PRINTING, BDD);
  if (checking && negate)
    die ("can not combine%s%s and%s", CHECKING, NEGATE);
# undef FORMULA
# undef AIGER
# undef DIMACS
# undef SAT
# undef TAUTOLOGY
# undef ENUMERATE
# undef BDD
# undef NEGATE
# undef PRINTING
# undef CHECKING
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

static Reader * input;
static Symbols * symbols;
static Circuit * primal, * dual;

static void setup_input (const char * input_name) {
  if (input_name) input = open_new_reader (input_name);
  else input = new_reader_from_stdin ();
  msg (1, "reading from '%s'", input->name);
}

static void parse (const char * input_name) {
  setup_input (input_name);
  assert (input);
  symbols = new_symbols ();
  Type type = get_file_type (input);
  if (type == FORMULA) {
    msg (1, "parsing input as formula");
    primal = parse_circuit (input, symbols);
  } else    if (type == DIMACS)  die ("can not parse DIMACS files yet");
  else { assert (type == AIGER); die ("can not parse AIGER files yet"); }
}

static void generate_dual () {
  assert (primal);
  assert (!dual);
  msg (1, "generating dual circuit");
  dual = negate_circuit (primal);
  if (negate) {
    SWAP (Circuit *, primal, dual);
    msg (1,
      "swapped dual and primal due since '%s' specified",
      (negate>0? "--negate" : "-n"));
  }
}

static const char * name_bdd (unsigned i) {
  assert (primal);
  assert (i < COUNT (primal->inputs));
  Gate * g = primal->inputs.start[i];
  Symbol * s = g->symbol;
  assert (s);
  const char * res = s->name;
  assert (res);
  return res;
}

static BDD * simulate_primal () {
  double start = process_time ();
  assert (primal);
  BDD * res = simulate_circuit (primal);
  double time = process_time ();
  double delta = time - start;
  msg (1, "BDD simulation of primal circuit in %.3f seconds", delta);
  return res;
}

static void check () {
  if (bdd) {
    msg (1, "checking with BDD engine");
    init_bdds ();
    BDD * b = simulate_primal ();
    if (sat) { 
      if (is_false_bdd (b)) printf ("s UNSATISFIABLE\n");
      else {
	printf ("s SATISFIABLE\n");
	fflush (stdout);
	printf ("v ");
	print_one_satisfying_cube (b, name_bdd);
	fputc ('\n', stdout);
	fflush (stdout);
      }
    } else {
      assert (tautology);
      if (is_true_bdd (b)) printf ("s TAUTOLOGY\n");
      else {
	printf ("s INVALID\n");
	fflush (stdout);
	printf ("v ");
	print_one_falsifying_cube (b, name_bdd);
	fputc ('\n', stdout);
	fflush (stdout);
      }
    }
    delete_bdd (b);
    reset_bdds ();
  } else die ("checking with SAT engine not implement yet");
}

static void print (const char * output_name) {
  Writer * output;
  if (output_name) {
    msg (1, "opening and writing to '%s'", output_name);
    output = open_new_writer (output_name);
  } else {
    msg (1, "opening and writing to '<stdout>'");
    output = new_writer_from_stdout ();
  }
  if (formula) {
    msg (1, "printing formula to '%s'", output->name);
    println_circuit_to_file (primal, output->file);
  } else if (dimacs) {
    CNF * f = new_cnf ();
    Encoding * e = new_encoding ();
    encode_circuit (primal, f, e, 0);
    print_dimacs_encoding_to_file (e, output->file);
    print_cnf_to_file (f, output->file);
    delete_cnf (f);
    delete_encoding (e);
  } else {
    assert (aiger);
    die ("printing of AIGER files not implemented yet");
  }
  delete_writer (output);
  msg (1, "finished writing '%s'",
    output_name ? output_name : "<stdout>");
}

static void all () {
  if (bdd) {
    msg (1, "enumerating with BDD engine");
    init_bdds ();
    BDD * b = simulate_primal ();
    if (negate) printf ("s ALL FALSIFYING ASSIGNMENTS\n");
    else printf ("s ALL SATISFYING ASSIGNMENTS\n");
    fflush (stdout);
    print_all_satisfying_cubes (b, name_bdd, "v ");
    delete_bdd (b);
    reset_bdds ();
  } else die ("enumerating with SAT engine not implement yet (use '-b')");
}

static void count () {
  if (bdd) {
    msg (1, "counting with BDD engine");
    init_bdds ();
    BDD * b = simulate_primal ();
    if (negate) printf ("s NUMBER FALSIFYING ASSIGNMENTS\n");
    else printf ("s NUMBER SATISFYING ASSIGNMENTS\n");
    fflush (stdout);
    unsigned num_inputs = COUNT (primal->inputs);
    if (num_inputs) {
      Number n;
      init_number (n);
      count_bdd (n, b, num_inputs-1);
      printf ("v ");
      println_number (n);
      clear_number (n);
    } else printf ("v 0\n");
    fflush (stdout);
    delete_bdd (b);
    reset_bdds ();
  } else die ("counting with SAT engine not implement yet (use '-b')");
}

static void reset () {
  if (primal)  delete_circuit (primal);
  if (dual)    delete_circuit (dual);
  if (symbols) delete_symbols (symbols);
}

static void setup_messages (const char * output_name) {
  if (!printing) return;
  if (output_name) return;
       if (dimacs)  message_prefix = "c ";
  else if (formula) message_prefix = "-- ";
  else if (aiger && verbosity) message_file = stderr;
}

int main (int argc, char ** argv) {
  const char * input_name = 0;
  const char * output_name = 0;
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
    else if (!strcmp (argv[i], "-b"))                bdd = -1;
    else if (!strcmp (argv[i], "--formula"))     formula = +1;
    else if (!strcmp (argv[i], "--aiger"))         aiger = +1;
    else if (!strcmp (argv[i], "--dimacs"))       dimacs = +1;
    else if (!strcmp (argv[i], "--negate"))       negate = +1;
    else if (!strcmp (argv[i], "--sat"))             sat = +1;
    else if (!strcmp (argv[i], "--tautology")) tautology = +1;
    else if (!strcmp (argv[i], "--enumerate")) enumerate = +1;
    else if (!strcmp (argv[i], "--bdd"))             bdd = +1;
    else if (argv[i][0] == '-' && argv[i][1] == '-') {
      if (!parse_option (argv[i]))
	die ("invalid long option '%s'", argv[i]);
    } else if (!strcmp (argv[i], "-o")) {
      if (++i == argc) die ("output file argument to '-o' missing'");
      if (output_name)
	die ("multiple output files '%s' and '%s'", output_name, argv[i]);
      output_name = argv[i];
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
  check_options (output_name);
  setup_messages (output_name);
  msg (1, "DualCount #SAT Solver");
  msg (1, "Copyright (C) 2017 Armin Biere Johannes Kepler University Linz");
  print_version ();
  set_signal_handlers ();
  print_options ();
#ifdef TEST
  test ();
#else
  parse (input_name);
  delete_reader (input);
  generate_dual ();
       if (checking)  check ();
  else if (printing)  print (output_name);
  else if (enumerate) all ();
  else                count ();
  reset ();
#endif
  reset_signal_handlers ();
  print_statistics ();
  assert (!allocated);
  return 0;
}
