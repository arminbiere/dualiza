#include "headers.h"

long bumped, searched;
long decisions, propagated, conflicts;
long backtracked, backjumped;

static double average (double a, double b) { return b ? a / b : 0; }
static double percent (double a, double b) { return b ? 100*a / b : 0; }

void print_statistics () {
  if (options.verbosity < 1) return;
  long bytes = maximum_resident_set_size ();
  double seconds = process_time ();
  if (conflicts || decisions || propagated) {
    msg (1, "%ld conflicts (%.0f per second)",
      conflicts, average (conflicts, seconds));
    msg (1, "%ld decisions (%.0f per second)",
      decisions, average (decisions, seconds));
    msg (1, "%ld propagations (%.3f million per second)",
      propagated, average (propagated / 1e6, seconds));
    if (backtracked)
      msg (1, "%ld backtracked (%.0f%% per conflict)",
      backtracked, percent (backtracked, conflicts));
    if (backjumped)
      msg (1, "%ld backjumped (%.0f%% per conflict)",
      backjumped, percent (backjumped, conflicts));
  }
  if (bumped || searched) {
    msg (1, "bumped %ld variables (%.2f per conflict)",
      bumped, average (bumped, conflicts));
    msg (1, "searched %ld variables (%.2f per decision)",
      searched, average (searched, decisions));
  }
  if (bdd_lookups || cache_lookups) {
    msg (1, "looked up %ld BDD nodes, %ld collisions (%.1f per look-up)",
      bdd_lookups, bdd_collisions,
      bdd_lookups ? bdd_collisions / (double) bdd_lookups : 0.0);
    msg (1, "looked up %ld BDD cache, %ld collisions (%.1f per look-up)",
      cache_lookups, cache_collisions,
      cache_lookups ? cache_collisions / (double) cache_lookups : 0.0);
  }
  if (symbol_lookups)
    msg (1, "looked up %ld symbols, %ld collisions (%.1f per look-up)",
      symbol_lookups, symbol_collisions,
      symbol_lookups ? symbol_collisions / (double) symbol_lookups : 0.0);
  msg (1, "maximum allocated %ld bytes (%.1f MB)",
    max_allocated, max_allocated / (double)(1<<20));
  msg (1, "maximum resident set size %ld bytes (%.1f MB)",
    bytes, bytes / (double)(1<<20));
  msg (1, "process time %.2f seconds", seconds);
}
