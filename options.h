/*------------------------------------------------------------------------*/
// List of long options available in all build configurations.

#define OPTIONS_ALL \
\
OPTION (bump,		1, "bump variables") \
OPTION (learn,          0, "learn clauses") \
OPTION (primal,         1, "use primal SAT engine only") \
OPTION (print,		1, "print model or number of all assignments") \
OPTION (verbosity,	0, "verbose level") \

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
