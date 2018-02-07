#include "headers.h"

Options options = {
#undef OPTION
#define OPTION(NAME,DEFAULT,DESCRIPTION) \
DEFAULT,
OPTIONS
};

void usage_options () {
  int len = 0, tmp;
  char buffer[80];
#undef OPTION
#define OPTION(NAME,DEFAULT,DESCRIPTION) \
  sprintf (buffer, "%s=%d", #NAME, (int) DEFAULT); \
  if ((tmp = strlen (buffer)) > len) len = tmp;
  OPTIONS
#undef OPTION
#define OPTION(NAME,DEFAULT,DESCRIPTION) \
  sprintf (buffer, "%s=%d", #NAME, (int) DEFAULT); \
  fputs ("  --", stdout); \
  fputs (buffer, stdout); \
  for (int i = strlen (buffer) - 5; i < len; i++) \
    fputc (' ', stdout); \
  fputs (DESCRIPTION, stdout); \
  fputc ('\n', stdout);
  OPTIONS
}

static void print_option (const char * name, int current, int usually) {
  if (usually != current) msg (1, "--%s=%d", name, current);
}

void print_options () {
#undef OPTION
#define OPTION(NAME,DEFAULT,DESCRIPTION) \
  print_option (#NAME, options.NAME, DEFAULT);
  OPTIONS
}

static int parse_option_aux (char * arg) {
  char * p = strchr (arg, '=');
  if (!p) return 0;
  *p++ = 0;
  char * q = p;
  if (!*p) return 0;
  while (*p)
    if (!isdigit (*p++))
      return 0;
  int res = 0;
#undef OPTION
#define OPTION(NAME,DEFAULT,DESCRIPTION) \
  if (!strcmp (arg, #NAME)) options.NAME = atoi (q), res = 1;
  OPTIONS
  if (!strcmp (arg, "eager") && options.eager) options.lazy = 0;
  if (!strcmp (arg, "lazy") && options.lazy) options.eager = 0;
  return res;
}

int parse_option (const char * arg) {
  if (arg[0] != '-') return 0;
  if (arg[1] != '-') return 0;
  if (arg[2] == 'n' && arg[3] == 'o' && arg[4] == '-') {
#undef OPTION
#define OPTION(NAME,DEFAULT,DESCRIPTION) \
    if (!strcmp (arg + 5, #NAME)) { options.NAME = 0; return 1; }
    OPTIONS
    return 0;
  }
  int len = strlen (arg + 2);
  char * tmp;
  ALLOC (tmp, len + 1);
  strcpy (tmp, arg + 2);
  int res = parse_option_aux (tmp);
  DEALLOC (tmp, len + 1);
  return res;
}
