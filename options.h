/*------------------------------------------------------------------------*/
// List of long options available in all build configurations.

#define OPTIONS_ALL \
 \
OPTION (block,        1, "use blocking clauses") \
OPTION (bump,         1, "bump variables (1=resolved, 2=reason)") \
OPTION (eager,        1, "eager blocking clauses (not '--lazy')") \
OPTION (eagerlimit,   2, "eager blocking clause size limit") \
OPTION (inputs,       0, "split on inputs first (in primal solver)") \
OPTION (keepglue,     3, "keep all clause of this glue") \
OPTION (keepsize,     3, "keep all clause of this size") \
OPTION (learn,        1, "learn clauses") \
OPTION (lazy,         0, "lazy blocking clauses (not '--eager')") \
OPTION (lazynlim,     1, "number lazy blocking clauses limit") \
OPTION (lazyslim,    20, "lazy blocking clause size limit") \
OPTION (phaseinit,    1, "initial default phase") \
OPTION (primal,       1, "use primal SAT engine only") \
OPTION (print,        1, "print model or number of all assignments") \
OPTION (reduce,       1, "garbage collect useless learned clauses") \
OPTION (reduceinc,  300, "reduce interval increment") \
OPTION (reduceinit, 2e3, "initial reduce interval") \
OPTION (rephase,      1, "enable reinitializing default phase") \
OPTION (rephaseint, 1e4, "reinitializing phase conflict interval") \
OPTION (restart,      1, "enable search restarts") \
OPTION (restartint,   2, "base restart interval") \
OPTION (reuse,        1, "reuse trail during restart") \
OPTION (subsume,      1, "eager blocking clause subsumption") \
OPTION (subsumelimit, 2, "limit on number of non-subsumed clauses")  \
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
