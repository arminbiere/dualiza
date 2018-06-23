#include "headers.h"

int sat_competition_mode;

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
"  -n | --negate          negate before printing\n"
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
"  -c | --count           force counting (default)\n"
"\n"
"By default the SAT engine is used, or alternatively the BDD engine.\n"
"\n"
"  -b | --bdd             use BDD engine instead of SAT engine\n"
"  --visualize            visualize generated BDD\n"
"\n"
"We are supporting projected model counting unless the 'project' option\n"
"is disabled .  Relevant variables are specified by the following option\n"
"(which can be repeated)\n"
"\n"
"  -r <var>[,<var>]*\n"
"\n"
"where '<var>' is either a symbolic name in case of formulas, the input\n"
"index in case of AIGER files (starting with '0') or positive integers\n"
"for DIMACS files.  Repeated variables are ignored.  For DIMACS files it\n"
"is also possible to specify the list of relevant variables as a comment\n"
"in the file.  Combining both ways to specify relevant variable for DIMACS\n"
"files gives an error.\n"
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
static int printing, checking, counting;
static int sat, tautology, enumerate;
static int bdd, limited, visualize;
static long limit;

static IntStack * relevant_ints;
static StrStack * relevant_strs;

# define FORMULA   (formula  >0?" '--formula'"  :(formula<0  ?" '-f'":""))
# define AIGER     (aiger    >0?" '--aiger'"    :(aiger<0    ?" '-a'":""))
# define DIMACS    (dimacs   >0?" '--dimacs'"   :(dimacs<0   ?" '-d'":""))
# define SAT       (sat      >0?" '--sat'"      :(sat<0      ?" '-s'":""))
# define TAUTOLOGY (tautology>0?" '--tautology'":(tautology<0?" '-t'":""))
# define ENUMERATE (enumerate>0?" '--enumerate'":(enumerate<0?" '-e'":""))
# define BDD       (bdd      >0?" '--bdd'"      :(bdd<0      ?" '-b'":""))
# define NEGATE    (negate   >0?" '--negate'"   :(negate<0   ?" '-n'":""))
# define COUNTING  (counting >0?" '--count'"    :(counting<0 ?" '-c'":""))

# define PRINTING  FORMULA,AIGER,DIMACS
# define CHECKING  SAT,TAUTOLOGY

static void check_options (const char * output_name) {
  printing = (formula != 0) + (aiger != 0) + (dimacs != 0);
  checking = (sat != 0) + (tautology != 0);
  int relevant = (relevant_ints || relevant_strs);
  if (printing > 1)
    die ("too many printing options:%s%s%s", PRINTING);
  if (checking > 1)
    die ("too many checking options:%s%s", CHECKING);
  if (printing && checking)
    die ("can not combine%s%s%s and%s%s", PRINTING, CHECKING);
  if (printing && enumerate)
    die ("can not combine%s%s%s and%s", PRINTING, ENUMERATE);
  if (printing && counting)
    die ("can not combine%s%s%s and%s", PRINTING, COUNTING);
  if (checking && enumerate)
    die ("can not combine%s%s and%s", CHECKING, ENUMERATE);
  if (checking && counting)
    die ("can not combine%s%s and%s", CHECKING, COUNTING);
  if (!printing && output_name)
    die ("output specified without printing option");
  if (printing && bdd)
    die ("can not use%s%s%s and%s", PRINTING, BDD);
  if (checking && negate)
    die ("can not combine%s%s and%s", CHECKING, NEGATE);
  if (!bdd && visualize)
    die ("can not use '--visualize' without BDD");
  if (checking && limited)
    die ("can not combine%s%s and '%ld'", CHECKING, limit);
  if (printing && limited)
    die ("can not combine%s%s%s and '%ld'", PRINTING, limit);
  if (options.annotate && !dimacs)
    die ("can not use '--annotate' without DIMACS printing");
  if (relevant_strs && relevant_ints)
    die ("can not combine '-r' for strings and integers");
  if (relevant && printing)
    die ("can not combine '-r' and%s%s%s", PRINTING);
  if (relevant && checking)
    die ("can not combine '-r' and%s%s (yet)", CHECKING);
  if (relevant && !options.project)
    die ("not use '-r' without '--project'");
}

static void init_mode () {
  if (formula)    msg (1, "formula printing mode due to%s", FORMULA);
  if (dimacs)     msg (1, "DIMACS printing mode due to%s", DIMACS);
  if (aiger)      msg (1, "AIGER printing mode due to%s", AIGER);
  if (sat)        msg (1, "satisfiability checking mode due to%s", SAT);
  if (tautology)  msg (1, "tautology checking mode due to%s", TAUTOLOGY);
  if (enumerate)  msg (1, "enumeration mode due to%s", ENUMERATE);
  if (!printing && !checking && !enumerate) {
    if (counting) msg (1, "counting mode due to%s", COUNTING);
    else          msg (1, "default counting mode"), counting = 1;
  }
  assert (checking + printing + (enumerate!=0) + (counting!=0) == 1);
  if (checking && options.project) {
    msg (1, "disabling projection due to%s", CHECKING);
    options.project = 0;
  }
  if (dimacs && options.polarity == 1) {
    msg (1,
      "disabling polarity based encoding due to%s "
      "(use '--polarity=2' to force it)", DIMACS);
    options.polarity = 0;
  }
  fix_options ();
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
static Circuit * primal_circuit;
static Circuit * dual_circuit;

static IntStack * relevant;

static void setup_input (const char * input_name) {
  if (input_name) input = open_new_reader (input_name);
  else input = new_reader_from_stdin ();
  msg (1, "reading from '%s'", input->name);
}

static void parse (const char * input_name) {
  setup_input (input_name);
  assert (input);
  symbols = new_symbols ();
  Info info = get_file_info (input);
  if (info == FORMULA) {
    if (relevant_ints)
      die ("reading formula with integer '-r' arguments");
    msg (1, "parsing input as formula");
    primal_circuit = parse_formula (input, symbols);
    if (relevant_strs)
      die ("relevant variables for formulas not implemented yet");
  } else if (info == DIMACS) {
    if (relevant_strs)
      die ("reading DIMACS file with symbolic '-r' arguments");
    if (relevant_ints && !PEEK (*relevant_ints, 0))
      die ("reading DIMACS file with zero in '-r' argument");
    if (checking) {
      msg (1, "switching to SAT solver competition mode");
      sat_competition_mode = 1;
    }
    msg (1, "parsing input as DIMACS file");
    primal_circuit = parse_dimacs (input, symbols, &relevant);
    if (relevant && relevant_ints)
      die ("found relevant variables in DIMACS file combined with '-r'");
    if (!relevant && relevant_ints) {
      int max_dimacs_var = (int) COUNT (primal_circuit->inputs);
      int max_relevant_var = TOP (*relevant_ints);
      if (max_dimacs_var < max_relevant_var)
	die ("variable '%d' in '-r' exceeds maximum DIMACS variable '%d'",
	  max_relevant_var, max_dimacs_var);
      relevant = relevant_ints;
      relevant_ints = 0;
    }
  } else {
    if (relevant_ints)
      die ("relevant variables for AIGER not implemented yet");
    assert (info == AIGER);
    msg (1, "parsing input as AIGER file");
    primal_circuit = parse_aiger (input, symbols);
  }
}

static void generate_dual_circuit () {
  assert (checking + printing + (enumerate!=0) + (counting!=0) == 1);
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
  const double start = process_time ();
  assert (primal_circuit);
  BDD * res = simulate_circuit (primal_circuit);
  const double simulated = process_time ();
  const double simulation_time = simulated - start;
  msg (1, "BDD simulation of circuit in %.3f seconds", simulation_time);
  if (relevant) {
    BDD * tmp = project_bdd (res, relevant);
    delete_bdd (res);
    res = tmp;
    const double projected = process_time ();
    const double projection_time = projected - simulated;
    msg (1, "BDD projection on %zd relevant variables in %.3f seconds",
      COUNT (*relevant), projection_time);
    const double total = projected - start;
    msg (1, "total BDD computation time of %.3f seconds", total);
  }
  if (visualize) {
    Name n = construct_name (primal_circuit, (GetName) name_circuit_input);
    visualize_bdd (res, n);
  }
  return res;
}

static int check () {

  assert (!relevant);
  assert (!relevant_ints);
  assert (!relevant_strs);

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
      msg (1, "primal CNF with %ld clauses", primal_cnf->irredundant);
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
      msg (1, "primal CNF with %ld clauses", primal_cnf->irredundant);
      msg (1, "dual CNF with %ld clauses", dual_cnf->irredundant);
    }
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (circuit, &inputs);
    Solver * solver = new_solver (primal_cnf, &inputs, 0, dual_cnf);
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
    if (options.annotate)
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
    Solver * solver = new_solver (cnf, &inputs, relevant, 0);
    if (limited) limit_number_of_partial_models (solver, limit);
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
    Solver * solver = new_solver (primal_cnf, &inputs, relevant, dual_cnf);
    if (limited) limit_number_of_partial_models (solver, limit);
    if (negate) printf ("ALL FALSIFYING ASSIGNMENTS\n");
    else printf ("ALL SATISFYING ASSIGNMENTS\n");
    dual_enumerate (solver, n);
    delete_solver (solver);
    RELEASE (inputs);
    delete_cnf (primal_cnf);
    delete_cnf (dual_cnf);
  }
}

static void init_relevant (unsigned num_inputs) {
  NEW (relevant);
  INIT (*relevant);
  for (int i = 1; (unsigned) i <= num_inputs; i++)
    PUSH (*relevant, i);
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
      if (!relevant) init_relevant (num_inputs);
      count_bdd (n, b, relevant);
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
    msg (1, "primal CNF with %ld clauses", cnf->irredundant);
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (primal_circuit, &inputs);
    Solver * solver = new_solver (cnf, &inputs, relevant, 0);
    if (limited) limit_number_of_partial_models (solver, limit);
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
    msg (1, "primal CNF with %ld clauses", primal_cnf->irredundant);
    msg (1, "dual CNF with %ld clauses", dual_cnf->irredundant);
    IntStack inputs;
    INIT (inputs);
    get_encoded_inputs (primal_circuit, &inputs);
    Solver * solver =
      new_solver (primal_cnf, &inputs, relevant, dual_cnf);
    if (limited) limit_number_of_partial_models (solver, limit);
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
  if (relevant)      { RELEASE (*relevant);      DELETE (relevant); }
  if (relevant_ints) { RELEASE (*relevant_ints); DELETE (relevant_ints); }
  if (relevant_strs) {
    while (!EMPTY (*relevant_strs))
      STRDEL (POP (*relevant_strs));
    RELEASE (*relevant_strs);
    DELETE (relevant_strs);
  }
}

static void setup_messages (const char * output_name) {
  if (!printing) return;
  if (output_name) return;
  if (formula) message_prefix = "-- ";
  else message_prefix = "c ";
  if (aiger && options.verbosity) message_file = stderr;
}

/*------------------------------------------------------------------------*/

static void sort_and_flush_relevant_ints () {

  int cmp (const void * p, const void * q) {
    return *(int*)p - *(int*)q;
    int a = * (int *) p, b = * (int *) q;
    return a - b;
  }

  const size_t n = COUNT (*relevant_ints);
  qsort (relevant_ints->start, n, sizeof (int), cmp);
  size_t i = 0;
  int prev = -1;
  for (size_t j = 0; j < n; j++) {
    int d = PEEK (*relevant_ints, j);
    if (d == prev) {
      LOG ("removing duplicated relevant entry '%d'", d);
      continue;
    }
    POKE (*relevant_ints, i, d);
    prev = d;
    i++;
  }
  RESIZE (*relevant_ints, i);
#ifndef NLOG
  for (size_t j = 0; j < i; j++)
    LOG ("sorted relevant %d", PEEK (*relevant_ints, j));
#endif
}

static void sort_and_flush_relevant_strs () {

  int cmp (const void * p, const void * q) {
    const char * a = * (char **) p, * b = * (char **) q;
    return strcmp (a, b);
  }

  const size_t n = COUNT (*relevant_strs);
  qsort (relevant_strs->start, n, sizeof (char*), cmp);
  size_t i = 0;
  const char * prev = 0;
  for (size_t j = 0; j < n; j++) {
    char * s = PEEK (*relevant_strs, j);
    if (!strcmp (s, prev)) {
      LOG ("removing duplicated relevant entry '%s'", s);
      STRDEL (s);
      continue;
    }
    POKE (*relevant_strs, i, s);
    prev = s;
    i++;
  }
  RESIZE (*relevant_strs, i);
#ifndef NLOG
  for (size_t j = 0; j < i; j++)
    LOG ("sorted relevant '%s'", PEEK (*relevant_strs, j));
#endif
}

/*------------------------------------------------------------------------*/

static void parse_relevant_ints (char * arg) {
  if (!relevant_ints) NEW (relevant_ints);
  for (char * p = arg, * end; *p; p = end + 1) {
    for (end = p; *end && *end != ','; end++)
      ;
    if (p == end)
      die ("two consecutive ',' in argument to '-r'");
    *end = 0;
    int res = 0, digits = 0;
    for (const char * q = p; q < end; q++) {
      if (!isdigit (*q))
	die ("non-digit character %s in argument to '-r'",
	  make_character_printable_as_string (*q));
      if (INT_MAX/10 < res)
INVALID: die ("invalid integer '%s' in argument to '-r'", p);
      res *= 10;
      int digit = *q - '0';
      if (digits++ && !res && !digit) goto INVALID;
      if (INT_MAX - digit < res) goto INVALID;
      res += digit;
    }
    assert (res >= 0);
    assert (atoi (p) == res);
    LOG ("pushing relevant %d", res);
    PUSH (*relevant_ints, res);
  }
}

static void parse_relevant_strs (char * arg) {
  if (!relevant_strs) NEW (relevant_strs);
  for (char * p = arg, * end; *p; p = end + 1) {
    for (end = p; *end && *end != ','; end++)
      ;
    if (p == end)
      die ("two consecutive ',' in argument to '-r'");
    *end = 0;
    if (!is_symbol_start (*p))
      die ("invalid first symbol character %s in argument to '-r'",
        make_character_printable_as_string (*p));
    for (const char * q = p + 1; q < end; q++) {
      if (!is_symbol_character (*p))
	die ("invalid symbol character %s in argument to '-r'",
	  make_character_printable_as_string (*q));
    }
    char * res;
    STRDUP (res, p);
    LOG ("pushing relevant '%s'", res);
    PUSH (*relevant_strs, res);
  }
}

static void parse_relevant_variables (const char * arg) {
  if (!*arg) die ("empty argument to '-r'");
  if (*arg == ',') die ("leading ',' in argument to '-r'");
  if (arg[strlen(arg)-1] == ',') die ("trailing ',' in argument to '-r'");
  size_t len = strlen (arg);
  char * tmp;
  ALLOC (tmp, len);
  strcpy (tmp, arg);
  if (isdigit (*arg)) {
    parse_relevant_ints (tmp);
    sort_and_flush_relevant_ints ();
  } else {
    parse_relevant_strs (tmp);
    sort_and_flush_relevant_strs ();
  }
  DEALLOC (tmp, len);
  if (!relevant_ints) return;
  if (!relevant_strs) return;
  assert (!EMPTY (*relevant_ints));
  assert (!EMPTY (*relevant_strs));
  die ("can not combine '-r %d' and '-r %s'",
    PEEK (*relevant_ints, 0), PEEK (*relevant_strs, 0));
}

/*------------------------------------------------------------------------*/

int main (int argc, char ** argv) {
  const char * input_name = 0;
  const char * output_name = 0;
  for (int i = 1; i < argc; i++) {
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
    else if (!strcmp (argv[i], "-c"))           counting = -1;
    else if (!strcmp (argv[i], "-b"))                bdd = -1;
    else if (!strcmp (argv[i], "--formula"))     formula = +1;
    else if (!strcmp (argv[i], "--aiger"))         aiger = +1;
    else if (!strcmp (argv[i], "--dimacs"))       dimacs = +1;
    else if (!strcmp (argv[i], "--negate"))       negate = +1;
    else if (!strcmp (argv[i], "--sat"))             sat = +1;
    else if (!strcmp (argv[i], "--tautology")) tautology = +1;
    else if (!strcmp (argv[i], "--enumerate")) enumerate = +1;
    else if (!strcmp (argv[i], "--counting"))   counting = +1;
    else if (!strcmp (argv[i], "--bdd"))             bdd = +1;
    else if (!strcmp (argv[i], "--visualize")) visualize = 1;
    else if (!strcmp (argv[i], "-r")) {
      if (++i == argc) die ("argument to '-r' missing'");
      parse_relevant_variables (argv[i]);
    } else if (argv[i][0] == '-' && argv[i][1] == '-') {
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
  }
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
  print_rules ();
  print_statistics ();
  assert (!stats.bytes.current);
  return res;
}
