#include "headers.h"

void print_statistics () {
  if (verbosity < 1) return;
  long bytes = maximum_resident_set_size ();
  double seconds = process_time ();
  msg (0, "looked up %ld BDDs, %ld collisions (%.1f per look-up)",
    bdd_lookups, bdd_collisions,
    bdd_lookups ? bdd_collisions / (double) bdd_lookups : 0.0);
  msg (0, "looked up %ld symbols, %ld collisions (%.1f per look-up)",
    symbol_lookups, symbol_collisions,
    symbol_lookups ? symbol_collisions / (double) symbol_lookups : 0.0);
  msg (0, "maximum allocated %ld bytes (%.1f MB)",
    max_allocated, max_allocated / (double)(1<<20));
  msg (0, "maximum resident set size %ld bytes (%.1f MB)",
    bytes, bytes / (double)(1<<20));
  msg (0, "process time %.2f seconds", seconds);
}
