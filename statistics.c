#include "headers.h"

void print_statistics () {
  if (verbosity < 1) return;
  long bytes = maximum_resident_set_size ();
  double seconds = process_time ();
  msg (0, "maximum allocated %ld bytes (%.1f MB)",
    max_allocated, max_allocated / (double)(1<<20));
  msg (0, "maximum resident set size %ld bytes (%.1f MB)",
    bytes, bytes / (double)(1<<20));
  msg (0, "process time %.2f seconds", seconds);
}
