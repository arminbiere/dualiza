#include "headers.h"

int has_suffix (const char * s, const char * t) {
  size_t k = strlen (s), l = strlen (t);
  return k >= l && !strcmp (s + k - l, t);
}

static FILE * open_pipe (int write, const char * fmt, const char * arg) {
#ifdef __MINGW32__
  die ("will not open pipe in MINGW environment");
  return 0;
#else
  long bytes = strlen (fmt);
  if (arg) bytes += strlen (arg);
  char * cmd;
  ALLOC (cmd, bytes);
  if (arg) sprintf (cmd, fmt, arg);
  else strcpy (cmd, fmt);
  const char * type = write ? "w" : "r";
  FILE * res = popen (cmd, type);
  DEALLOC (cmd, bytes);
  if (!res) die ("'popen (\"%s\", \"%s\")' failed", cmd, type);
  return res;
#endif
}

FILE * read_pipe (const char * fmt, const char * arg) {
  return open_pipe (0, fmt, arg);
}

FILE * write_pipe (const char * fmt, const char * arg) {
  return open_pipe (1, fmt, arg);
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int file_exists (const char * name) {
  struct stat buf;
  return !stat (name, &buf);
}

unsigned primes[num_primes] = {
  1000003u, 10000019u, 100000007u, 500000003u,
  1000000007u, 1500000001u, 2000000011u, 3000000019u
};

int is_space_character (int ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\n' || ch == '\r';
} 

void * first_non_zero_byte (void * p, size_t size) {
  char * end = size + (char*) p;
  for (const char * q = p; q != end; q++)
    if (*q) return (void*) q;
  return 0;
}

const char * make_character_printable_as_string (char ch) {
  static char buffer[16];
  if (isprint (ch)) sprintf (buffer, "'%c'", ch);
  else sprintf (buffer, "code 0x%d", ch);
  return buffer;
}

double average (double a, double b) { return b ? a / b : 0; }
double percent (double a, double b) { return b ? 100*a / b : 0; }
