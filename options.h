/*------------------------------------------------------------------------*/
// List of long options available in all build configurations.

#define OPTIONS_ALL \
 \
OPTION (annotate,     0, "annotate generated") \
OPTION (block,        1, "use blocking clauses") \
OPTION (bump,         1, "bump variables (1=resolved, 2=reason)") \
OPTION (blocklimit,   2, "blocking clause size limit") \
DBGOPT (check,        0, "enable expensive assertion checking") \
OPTION (discount,     1, "discount models instead of backtracking") \
OPTION (discountmax,  0, "maximum number of discounted models") \
OPTION (dual,         1, "dual SAT engine too (opposite of '--primal')") \
OPTION (keepglue,     3, "keep all clause of this glue") \
OPTION (keepsize,     3, "keep all clause of this size") \
OPTION (learn,        1, "learn clauses") \
OPTION (phaseinit,    1, "initial default phase") \
OPTION (primal,       0, "primal SAT engine only (opposite of '--dual')") \
OPTION (print,        1, "print model or number of all assignments") \
OPTION (reduce,       1, "garbage collect useless learned clauses") \
OPTION (reduceinc,  300, "reduce interval increment") \
OPTION (reduceinit, 2e3, "initial reduce interval") \
OPTION (project,      1, "project on relevant variables") \
OPTION (relevant,     0, "always split on relevant variables first") \
OPTION (restart,      1, "enable search restarts") \
OPTION (restartint,   2, "base restart interval") \
OPTION (reuse,        1, "reuse trail during restart") \
OPTION (subsume,      1, "eager blocking clause subsumption") \
OPTION (subsumelimit, 4, "limit on number of non-subsumed clauses")  \
OPTION (verbosity,    0, "verbose level") \

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
int parse_option (const char * arg);
void fix_options ();

#ifndef OPTION
typedef struct Options Options;
struct Options {
#define OPTION(NAME, DEFAULT, DESCRIPTION) \
int NAME;
OPTIONS
};
extern Options options;
#else
OPTIONS
#endif
