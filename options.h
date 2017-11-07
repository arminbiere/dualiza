/*------------------------------------------------------------------------*/
// List of long options available in all build configurations.

#define OPTIONS_ALL \
OPTION (primal,         1, use primal SAT engine only) \
OPTION (printnumber,	1, really print number of all assignments) \
OPTION (verbosity,	0, verbose level) \

/*------------------------------------------------------------------------*/
// List of long options only available in logging build ('-l').
#ifndef NLOG
#define OPTIONS_LOG \
OPTION (logging,	0, logging level)
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
#define OPTION(NAME, DEFAULT, DESCRIPTION) \
extern int NAME;
#endif

OPTIONS
