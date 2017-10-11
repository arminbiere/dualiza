#include "headers.h"

void print_statistics () {
  long bytes = maximum_resident_set_size ();
  double seconds = process_time ();
  msg ("maximum allocated %ld bytes (%.1f MB)",
    max_allocated, max_allocated / (double)(1<<20));
  msg ("maximum resident set size %ld bytes (%.1f MB)",
    bytes, bytes / (double)(1<<20));
  msg ("process time %.2f seconds", seconds);
}
