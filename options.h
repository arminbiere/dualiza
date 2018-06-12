/*------------------------------------------------------------------------*/
// List of long options available in all build configurations.

#define OPTIONS_ALL \
 \
OPTION (annotate,     0, "annotate generated") \
OPTION (block,        0, "use blocking clauses") \
OPTION (bump,         1, "bump variables (1=resolved, 2=reason)") \
OPTION (blocklimit,   2, "blocking clause size limit") \
OPTION (discount,     1, "discount models instead of backtracking") \
OPTION (discountmax,  1, "maximum number of flipped levels discounted") \
OPTION (dual,         0, "dual SAT engine too (opposite of '--primal')") \
OPTION (keepglue,     3, "keep all clause of this glue") \
OPTION (keepsize,     3, "keep all clause of this size") \
OPTION (learn,        1, "learn clauses") \
OPTION (phaseinit,    1, "initial default phase") \
OPTION (primal,       1, "primal SAT engine only (opposite of '--dual')") \
OPTION (print,        1, "print model or number of all assignments") \
OPTION (reduce,       1, "garbage collect useless learned clauses") \
OPTION (reduceinc,  300, "reduce interval increment") \
OPTION (reduceinit, 2e3, "initial reduce interval") \
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

#define OPTIONS \
OPTIONS_ALL \
OPTIONS_LOG

extern int solving_option_set;

void usage_options ();
void print_options ();
int parse_option (const char * arg);

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
