#include "headers.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

double process_time () {
  struct rusage u;
  double res;
  if (getrusage (RUSAGE_SELF, &u)) return 0;
  res = u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec;  // user time
  res += u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec; // + system time
  return res;
}

long maximum_resident_set_size () {
  struct rusage u;
  if (getrusage (RUSAGE_SELF, &u)) return 0;
  return ((long) u.ru_maxrss) << 10;
}

long current_resident_set_size () {
  char path[40];
  sprintf (path, "/proc/%ld/statm", (long) getpid ());
  FILE * file = fopen (path, "r");
  if (!file) return 0;
  long dummy, rss;
  int scanned = fscanf (file, "%ld %ld", &dummy, &rss);
  fclose (file);
  return scanned == 2 ? rss * sysconf (_SC_PAGESIZE) : 0;
}
