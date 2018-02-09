#include "headers.h"

static void usage () {
fputs (
"usage: dualiza [ <option> ... ] [ <file> ] [ <limit> ]\n"
"\n"
"where '<option>' is one of the following flags\n"
"\n"
"  -h | --help            print this command line option summary\n"
"  --version              print version\n"
"  -v                     increase verbosity\n"
#ifndef NLOG       
"  -l                     enable logging\n"
#endif             
"\n"
"The '<file>' argument is a path to a file of the following formats\n"
"\n"
"  <formula>              ASCII format with standard operators\n"
"  <aiger>                and-inverter graph circuit in AIGER format\n"
"  <dimacs>               CNF in DIMACS format\n"
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
"  -f | --formula         print formula\n"
"  -a | --aiger           print AIGER\n"
"  -d | --dimacs          print DIMACS\n"
"\n"
"to '<stdout>' or optionally to a (compressed) file specified with\n"
"\n"
"  -o <file>\n"
"\n"
"or checked to have a model or counter-model which are printed with\n"
"\n"
"  -s | --sat             only check to be satisfiable\n"
"  -t | --tautology       only check to be tautological\n"
"  -e | --enumerate       enumerate and print all models\n"
"\n"
"By default the SAT engine is used, or alternatively the BDD engine.\n"
"\n"
"  -b | --bdd             use BDD engine instead of SAT engine\n"
"  --visualize            visualize generated BDD\n"
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

int sat_competition_mode;
static int formula, aiger, dimacs, negate;
static int printing, checking, counting;
static int sat, tautology, enumerate;
static int bdd, limited, visualize;
static long limit;
 
# define FORMULA   (formula  >0?" '--formula'"  :(formula<0  ?" '-f'":""))
# define AIGER     (aiger    >0?" '--aiger'"    :(aiger<0    ?" '-a'":""))
# define DIMACS    (dimacs   >0?" '--dimacs'"   :(dimacs<0   ?" '-d'":""))
# define SAT       (sat      >0?" '--sat'"      :(sat<0      ?" '-s'":""))
# define TAUTOLOGY (tautology>0?" '--tautology'":(tautology<0?" '-t'":""))
# define ENUMERATE (enumerate>0?" '--enumerate'":(enumerate<0?" '-e'":""))
# define BDD       (bdd      >0?" '--bdd'"      :(bdd<0      ?" '-b'":""))
# define NEGATE    (negate   >0?" '--negate'"   :(negate<0   ?" '-n'":""))

# define PRINTING  FORMULA,AIGER,DIMACS
# define CHECKING  SAT,TAUTOLOGY

static void check_options (const char * output_name) {
  printing = (formula != 0) + (aiger != 0) + (dimacs != 0);
  checking = (sat != 0) + (tautology != 0);
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
    die ("output specified without printing option");
  if (printing && bdd)
    die ("can not use%s%s%s and%s", PRINTING, BDD);
  if (checking && negate)
    die ("can not combine%s%s and%s", CHECKING, NEGATE);
  if (!bdd && visualize)
    die ("can not use '--visualize' without BDD");
}

static void init_mode () {
  counting = !printing && !checking && !enumerate;
  if (formula)   msg (1, "formula printing mode due to%s", FORMULA);
  if (dimacs)    msg (1, "DIMACS printing mode due to%s", DIMACS);
  if (aiger)     msg (1, "AIGER printing mode due to%s", AIGER);
  if (sat)       msg (1, "satisfiability checking mode due to%s", SAT);
  if (tautology) msg (1, "tautology checking mode due to%s", TAUTOLOGY);
  if (enumerate) msg (1, "enumeration mode due to%s", ENUMERATE);
  if (counting)  msg (1, "default counting mode");
  assert (checking + printing + (enumerate!=0) + counting == 1);
}

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
static Circuit * primal_circuit, * dual_circuit;

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
    primal_circuit = parse_formula (input, symbols);
  } else if (type == DIMACS) {
    if (checking) {
      msg (1, "switching to SAT solver competition mode");
      sat_competition_mode = 1;
    }
    msg (1, "parsing input as DIMACS file");
    primal_circuit = parse_dimacs (input, symbols);
  } else {
    assert (type == AIGER);
    msg (1, "parsing input as AIGER file");
    primal_circuit = parse_aiger (input, symbols);
  }
}

static void generate_dual_circuit () {
  assert (checking + printing + (enumerate!=0) + counting == 1);
  assert (primal_circuit);
  assert (!dual_circuit);
  if (negate) {
    assert (!checking);
    if (printing)
      msg (1,
"generating dual circuit for printing");
    else if (enumerate)
      msg (1,
"generating dual circuit for enumeration");
    else
      assert (counting), msg (1,
"generating dual circuit for counting");
  } else {
    if (checking) {
      if (bdd) {
	msg (1,
"using primal circuit for non-negated checking with BDD engine");
	return;
      } else if (options.primal && !tautology) {
	msg (1,
"using only primal circuit for primal checking with SAT engine");
	return;
      } else
        msg (1,
"generating dual circuit for dual checking with SAT engine");
    } else if (printing) {
      msg (1,
"using primal circuit for non-negated printing");
      return;
    } else if (enumerate) {
      if (bdd) {
	msg (1,
"using primal circuit for non-negated enumeration with BDD engine");
	return;
      } else if (options.primal && !negate) {
	msg (1,
"using only primal circuit for primal enumeration with SAT engine");
	return;
      } else
        msg (1,
"generating dual circuit for enumeration with SAT engine");
    } else {
      assert (counting);
      if (bdd) {
	msg (1,
"using primal circuit for non-negated counting with BDD engine");
	return;
      } else if (options.primal && !negate) {
	msg (1,
"using only primal circuit for primal counting with SAT engine");
	return;
      } else
        msg (1,
"generating dual circuit for counting with SAT engine");
    }
  }
  dual_circuit = negate_circuit (primal_circuit);
  if (negate) {
    SWAP (Circuit *, primal_circuit, dual_circuit);
    msg (1,
      "swapped dual and primal circuit since '%s' specified",
      (negate>0? "--negate" : "-n"));
  }
}

static const char *
name_circuit_input (Circuit * circuit, int i) {
  Gate * g = decode_literal (circuit, i);
  assert (g);
  assert (!SIGN (g));
  Symbol * s = g->symbol;
  assert (s);
  const char * res = s->name;
  assert (res);
  return res;
}

static BDD * simulate_primal () {
  double start = process_time ();
  assert (primal_circuit);
  BDD * res = simulate_circuit (primal_circuit);
  double time = process_time ();
  double delta = time - start;
  msg (1, "BDD simulation of primal circuit in %.3f seconds", delta);
  if (visualize) {
    Name n = construct_name (primal_circuit, (GetName) name_circuit_input);
    visualize_bdd (res, n);
  }
  return res;
}

static int check () {
  int res = 0;
  if (bdd) {
    msg (1, "checking with BDD engine");
    init_bdds ();
    BDD * b = simulate_primal ();
    Name n = construct_name (primal_circuit, (GetName) name_circuit_input);
    if (sat) { 
      if (sat_competition_mode) fputs ("s ", stdout);
      if (is_false_bdd (b)) {
	res = 20;
	printf ("UNSATISFIABLE\n");
      } else {
	res = 10;
	printf ("SATISFIABLE\n");
	fflush (stdout);
	if (options.print) {
	  if (sat_competition_mode) fputs ("v ", stdout);
	  print_one_satisfying_cube (b, n);
	  if (sat_competition_mode) {
	    if (!is_true_bdd (b)) fputc ('\n', stdout);
	    fputs ("v 0", stdout);
	  }
	  fputc ('\n', stdout);
	  fflush (stdout);
	}
      }
    } else {
      assert (tautology);
      if (is_true_bdd (b)) printf ("TAUTOLOGY\n");
      else {
	printf ("INVALID\n");
	fflush (stdout);
	if (options.print) {
	  print_one_falsifying_cube (b, n);
	  fputc ('\n', stdout);
	  fflush (stdout);
	}
      }
    }
    delete_bdd (b);
    reset_bdds ();
  } else {
    Circuit * circuit;
    CNF * primal_cnf, * dual_cnf = 0;
    if (options.primal) {
      msg (1, "checking with primal SAT engine");
      primal_cnf = new_cnf (0);
      circuit = tautology ? dual_circuit : primal_circuit;
      encode_circuit (circuit, primal_cnf);
    } else {
      msg (1, "checking with dual SAT engine");
      primal_cnf = new_cnf (0);
      dual_cnf = new_cnf (1);
      if (tautology) {
	circuit = dual_circuit;
	msg (2,
	  "swapping role of primal and dual circuit for tautology checking");
	encode_circuits (circuit, primal_circuit, primal_cnf, dual_cnf);
      } else {
	circuit = primal_circuit;
	encode_circuits (circuit, dual_circuit, primal_cnf, dual_cnf);
      }
    }
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (circuit, &inputs);
    Solver * solver = new_solver (primal_cnf, &inputs, dual_cnf);
    if (options.primal) res = primal_sat (solver);
    else res = dual_sat (solver);
    if (sat) {
      if (sat_competition_mode) fputs ("s ", stdout);
      if (res == 20) printf ("UNSATISFIABLE\n");
      else if (res == 10) printf ("SATISFIABLE\n");
      else printf ("UNKNOWN\n");
    } else {
      assert (tautology);
      if (res == 20) printf ("TAUTOLOGY\n");
      else if (res == 10) printf ("INVALID\n");
      else printf ("UNKNOWN\n");
    }
    fflush (stdout);
    if (res == 10 && options.print) {
      int printed = 0;
      for (int * p = inputs.start; p < inputs.top; p++) {
	const int idx = *p, val = deref (solver, idx);
	if (!val) continue;
	if (printed++) fputc (' ', stdout);
	else if (sat_competition_mode) fputs ("v ", stdout);
	if (val < 0) fputc ((sat_competition_mode ? '-': '!'), stdout);
	fputs (name_circuit_input (primal_circuit, idx), stdout);
      }
      if (sat_competition_mode) {
	if (printed) fputc ('\n', stdout);
	fputs ("v 0", stdout);
      }
      fputc ('\n', stdout);
    }
    delete_solver (solver);
    RELEASE (inputs);
    delete_cnf (primal_cnf);
    if (dual_cnf) delete_cnf (dual_cnf);
  }
  return res;
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
    println_circuit_to_file (primal_circuit, output->file);
  } else if (dimacs) {
    CNF * cnf = new_cnf (0);
    encode_circuit (primal_circuit, cnf);
    print_dimacs_encoding_to_file (primal_circuit, output->file);
    print_cnf_to_file (cnf, output->file);
    delete_cnf (cnf);
  } else {
    assert (aiger);
    die ("printing of AIGER files not implemented yet");
  }
  delete_writer (output);
  msg (1, "finished writing '%s'",
    output_name ? output_name : "<stdout>");
}

static void all () {
  Name n = construct_name (primal_circuit, (GetName) name_circuit_input);
  if (bdd) {
    msg (1, "enumerating with BDD engine");
    init_bdds ();
    BDD * b = simulate_primal ();
    if (negate) printf ("ALL FALSIFYING ASSIGNMENTS\n");
    else printf ("ALL SATISFYING ASSIGNMENTS\n");
    fflush (stdout);
    print_all_satisfying_cubes (b, n);
    delete_bdd (b);
    reset_bdds ();
  } else if (options.primal) {
    msg (1, "enumerating with primal SAT engine");
    CNF * cnf = new_cnf (0);
    encode_circuit (primal_circuit, cnf);
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (primal_circuit, &inputs);
    Solver * solver = new_solver (cnf, &inputs, 0);
    if (negate) printf ("ALL FALSIFYING ASSIGNMENTS\n");
    else printf ("ALL SATISFYING ASSIGNMENTS\n");
    primal_enumerate (solver, n);
    delete_solver (solver);
    RELEASE (inputs);
    delete_cnf (cnf);
  } else {
    msg (1, "enumerating with dual SAT engine");
    CNF * primal_cnf = new_cnf (0);
    CNF * dual_cnf = new_cnf (1);
    encode_circuits (primal_circuit, dual_circuit, primal_cnf, dual_cnf);
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (primal_circuit, &inputs);
    Solver * solver = new_solver (primal_cnf, &inputs, dual_cnf);
    if (negate) printf ("ALL FALSIFYING ASSIGNMENTS\n");
    else printf ("ALL SATISFYING ASSIGNMENTS\n");
    dual_enumerate (solver, n);
    delete_solver (solver);
    RELEASE (inputs);
    delete_cnf (primal_cnf);
    delete_cnf (dual_cnf);
  }
}

static void count () {
  if (bdd) {
    msg (1, "counting with BDD engine");
    init_bdds ();
    BDD * b = simulate_primal ();
    if (negate) printf ("NUMBER FALSIFYING ASSIGNMENTS\n");
    else printf ("NUMBER SATISFYING ASSIGNMENTS\n");
    fflush (stdout);
    unsigned num_inputs = COUNT (primal_circuit->inputs);
    if (num_inputs) {
      double start = process_time ();
      Number n;
      init_number (n);
      count_bdd (n, b, num_inputs);
      double time = process_time ();
      double delta = time - start;
      msg (1,
"BDD solution counting of primal circuit in %.3f seconds", delta);
      if (options.print) println_number (n), fflush (stdout);
      clear_number (n);
    } else printf ("%d\n", is_true_bdd (b));
    fflush (stdout);
    delete_bdd (b);
    reset_bdds ();
  } else if (options.primal) {
    msg (1, "counting with primal SAT engine");
    CNF * cnf = new_cnf (0);
    encode_circuit (primal_circuit, cnf);
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (primal_circuit, &inputs);
    Solver * solver = new_solver (cnf, &inputs, 0);
    Number n;
    init_number (n);
    primal_count (n, solver);
    if (negate) printf ("NUMBER FALSIFYING ASSIGNMENTS\n");
    else printf ("NUMBER SATISFYING ASSIGNMENTS\n");
    fflush (stdout);
    if (options.print) println_number (n), fflush (stdout);
    clear_number (n);
    delete_solver (solver);
    RELEASE (inputs);
    delete_cnf (cnf);
  } else {
    msg (1, "counting with dual SAT engine");
    CNF * primal_cnf = new_cnf (0);
    CNF * dual_cnf = new_cnf (1);
    encode_circuits (primal_circuit, dual_circuit, primal_cnf, dual_cnf);
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (primal_circuit, &inputs);
    Solver * solver = new_solver (primal_cnf, &inputs, dual_cnf);
    Number n;
    init_number (n);
    dual_count (n, solver);
    if (negate) printf ("NUMBER FALSIFYING ASSIGNMENTS\n");
    else printf ("NUMBER SATISFYING ASSIGNMENTS\n");
    fflush (stdout);
    if (options.print) println_number (n), fflush (stdout);
    clear_number (n);
    delete_solver (solver);
    RELEASE (inputs);
    delete_cnf (primal_cnf);
    delete_cnf (dual_cnf);
  }
}

static void init () {
  generate_dual_circuit ();
}

static void reset () {
  if (primal_circuit)  delete_circuit (primal_circuit);
  if (dual_circuit)    delete_circuit (dual_circuit);
  if (symbols)         delete_symbols (symbols);
}

static void setup_messages (const char * output_name) {
  if (!printing) return;
  if (output_name) return;
  if (formula) message_prefix = "-- ";
  else message_prefix = "c ";
  if (aiger && options.verbosity) message_file = stderr;
}

int main (int argc, char ** argv) {
  const char * input_name = 0;
  const char * output_name = 0;
  for (int i = 1; i < argc; i++)
    if (!strcmp (argv[i], "-h") ||
        !strcmp (argv[i], "--help")) usage (), exit (0);
    else if (!strcmp (argv[i], "--version"))
      printf ("%s\n", get_version ()), exit (0);
    else if (!strcmp (argv[i], "-v")) options.verbosity++;
#ifndef NLOG
    else if (!strcmp (argv[i], "-l")) options.logging++;
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
    else if (!strcmp (argv[i], "--visualize")) visualize = 1;
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
  if (options.logging) options.verbosity = INT_MAX;
#endif
  check_options (output_name);
  setup_messages (output_name);
  msg (1,
    "Dualiza #SAT Solver");
  msg (1,
    "Copyright (C) 2017-2018 Armin Biere Johannes Kepler University Linz");
  print_version ();
  init_mode ();
  set_signal_handlers ();
  print_options ();
  parse (input_name);
  delete_reader (input);
  init ();
  int res = 0;
       if (checking)  res = check ();
  else if (printing)  print (output_name);
  else if (enumerate) all ();
  else                count ();
  reset ();
  reset_signal_handlers ();
  print_statistics ();
  assert (!stats.bytes.current);
  return res;
}
