/*------------------------------------------------------------------------*/
// List of long options (both in debugging and non-debugging build).

#define OPTIONS_NDEBUG \
OPTION (verbosity,	0, verbose level) \

/*------------------------------------------------------------------------*/
// List of long options only available in debugging build ('-g').

#define OPTIONS_DEBUG \
OPTION (logging,	0, logging level) \

/*------------------------------------------------------------------------*/

#define OPTIONS \
OPTIONS_NDEBUG \
OPTIONS_DEBUG

void usage_options ();
void print_options ();
int parse_option (const char * arg);

#ifndef OPTION
#define OPTION(NAME, DEFAULT, DESCRIPTION) \
extern int NAME;
#endif

OPTIONS
