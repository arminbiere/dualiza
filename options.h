#define OPTIONS \
OPTION (verbosity,	0, verbose level) \
OPTION (logging,	0, logging level) \
OPTION (dsop,		0, generate deterministic sum-of-produces)

void usage_options ();
void print_options ();
int parse_option (const char * arg);

#ifndef OPTION
#define OPTION(NAME, DEFAULT, DESCRIPTION) \
extern int NAME;
#endif

OPTIONS
