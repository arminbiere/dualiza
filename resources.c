#include "headers.h"

#ifdef __MINGW32__
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

double process_time () {
  double res = 0;
#ifdef __MINGW32__
  FILETIME c, e, u, s;
  if (GetProcessTimes (GetCurrentProcess (), &c, &e, &u, &s)) {
    res  = s.dwLowDateTime | (__int64) s.dwHighDateTime << 32;
    res += u.dwLowDateTime | (__int64) u.dwHighDateTime << 32;
    res *= 1e-7;
  }
#else
  struct rusage u;
  if (!getrusage (RUSAGE_SELF, &u)) {
    res  = u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec;
    res += u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
  }
#endif
  return res;
}

double maximum_resident_set_size () {
#ifdef __MINGW32__
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo (GetCurrentProcess (), &pmc, sizeof (pmc))) {
     return pmc.PeakWorkingSetSize;
  } else return -1;
#else
  struct rusage u;
  if (getrusage (RUSAGE_SELF, &u)) return 0;
  return ((long) u.ru_maxrss) << 10;
#endif
}

double current_resident_set_size () {
#ifdef __MINGW32__
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo (GetCurrentProcess (), &pmc, sizeof (pmc))) {
     return pmc.WorkingSetSize;
  } else return -1;
#else
  char path[40];
  sprintf (path, "/proc/%ld/statm", (long) getpid ());
  FILE * file = fopen (path, "r");
  if (!file) return 0;
  long dummy, rss;
  int scanned = fscanf (file, "%ld %ld", &dummy, &rss);
  fclose (file);
  return scanned == 2 ? rss * sysconf (_SC_PAGESIZE) : 0;
#endif
}
