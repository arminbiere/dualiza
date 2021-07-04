/*------------------------------------------------------------------------*/
// List of long options available in all build configurations.

// *INDENT-OFF*

#define OPTIONS_ALL \
 \
OPTION (annotate,     0, "annotate generated") \
OPTION (block,        1, "use blocking clauses") \
OPTION (bump,         1, "bump variables (1=resolved, 2=reason)") \
OPTION (blocklimit,   2, "blocking clause size limit") \
DBGOPT (check,        0, "enable expensive assertion checking") \
OPTION (elim,         1, "enabled bounded variable elimination") \
OPTION (elimclslim, 100, "clause size limit for variable elimination") \
OPTION (elimocclim,  10, "occurrence limit for variable elimination") \
OPTION (discount,     1, "discount models instead of backtracking") \
OPTION (discountmax,  0, "maximum number of discounted models") \
OPTION (dual,         1, "enable dual SAT engine (opposite of '--primal')") \
OPTION (flatten,      1, "flatten circuit before encoding") \
OPTION (keepglue,     3, "keep all clause of this glue") \
OPTION (keepsize,     3, "keep all clause of this size") \
OPTION (learn,        1, "learn clauses") \
OPTION (phaseinit,    1, "initial default phase") \
OPTION (primal,       0, "primal SAT engine only (opposite of '--dual')") \
OPTION (print,        1, "print model or number of all assignments") \
OPTION (reduce,       1, "garbage collect useless learned clauses") \
OPTION (reduceinc,  300, "reduce interval increment") \
OPTION (reduceinit, 2e3, "initial reduce interval") \
OPTION (polarity,     1, "use polarity based CNF encoding (2=force)") \
OPTION (project,      1, "project on relevant variables") \
OPTION (relevant,     0, "always split on relevant variables first") \
OPTION (restart,      1, "enable search restarts") \
OPTION (restartint,   2, "base restart interval") \
OPTION (reuse,        1, "reuse trail during restart") \
OPTION (subsume,      1, "clause subsumption") \
OPTION (sublearned,   1, "eager subsume learned clause subsumption") \
OPTION (sublearnlim,  4, "limit on number of non-subsumed clauses")  \
OPTION (verbosity,    0, "verbose level") \

// *INDENT-ON*

/*------------------------------------------------------------------------*/
// List of long options only available in logging build ('-l').
#ifndef NLOG
#define OPTIONS_LOG \
OPTION (logging,	0, "logging level")
#else
#define OPTIONS_LOG /**/
#endif
/*------------------------------------------------------------------------*/
#ifdef NDEBUG
#define DBGOPT(ARGS...) /**/
#else
#define DBGOPT OPTION
#endif
/*------------------------------------------------------------------------*/
#define OPTIONS \
OPTIONS_ALL \
OPTIONS_LOG
void usage_options ();
void print_options ();
int parse_option (const char *arg);
void fix_options ();

#ifndef OPTION
typedef struct Options Options;
struct Options
{
#define OPTION(NAME, DEFAULT, DESCRIPTION) \
int NAME;
OPTIONS};
extern Options options;
#else
OPTIONS
#endif
