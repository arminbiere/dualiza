#include "headers.h"

Stats stats;

static double average (double a, double b) { return b ? a / b : 0; }
static double percent (double a, double b) { return b ? 100*a / b : 0; }

void print_statistics () {
  if (options.verbosity < 1) return;
  long resident = maximum_resident_set_size ();
  double seconds = process_time ();
  if (stats.conflicts || stats.decisions || stats.propagated) {
    msg (1, "%ld conflicts (%.0f per second)",
      stats.conflicts, average (stats.conflicts, seconds));
    msg (1, "%ld decisions (%.0f per second)",
      stats.decisions, average (stats.decisions, seconds));
    msg (1, "%ld propagations (%.3f million per second)",
      stats.propagated, average (stats.propagated / 1e6, seconds));
    if (stats.backtracked)
      msg (1, "%ld backtracked (%.0f%% per conflict)",
      stats.backtracked, percent (stats.backtracked, stats.conflicts));
    if (stats.backjumped)
      msg (1, "%ld backjumped (%.0f%% per conflict)",
      stats.backjumped, percent (stats.backjumped, stats.conflicts));
    if (stats.reductions)
      msg (1, "%ld reductions (%.0f%% per learned)",
      stats.reductions, percent (stats.reductions, stats.learned));
  }
  if (stats.bumped || stats.searched) {
    msg (1, "bumped %ld variables (%.2f per conflict)",
      stats.bumped, average (stats.bumped, stats.conflicts));
    msg (1, "searched %ld variables (%.2f per decision)",
      stats.searched, average (stats.searched, stats.decisions));
  }
  if (stats.bdd.node.lookups || stats.bdd.cache.lookups) {
    msg (1, "looked up %ld BDD nodes, %ld collisions (%.1f per look-up)",
      stats.bdd.node.lookups, stats.bdd.node.collisions,
      stats.bdd.node.lookups ?
        stats.bdd.node.collisions/(double)stats.bdd.node.lookups : 0.0);
    msg (1, "looked up %ld BDD cache, %ld collisions (%.1f per look-up)",
      stats.bdd.cache.lookups, stats.bdd.cache.collisions,
      stats.bdd.cache.lookups ?
        stats.bdd.cache.collisions/(double)stats.bdd.cache.lookups : 0.0);
  }
  if (stats.symbol.lookups)
    msg (1, "looked up %ld symbols, %ld collisions (%.1f per look-up)",
      stats.symbol.lookups, stats.symbol.collisions,
      stats.symbol.lookups ?
        stats.symbol.collisions/(double) stats.symbol.lookups : 0.0);
  msg (1, "maximum allocated %ld bytes (%.1f MB)",
    stats.bytes.max, stats.bytes.max / (double)(1<<20));
  msg (1, "maximum resident set size %ld bytes (%.1f MB)",
    resident, resident / (double)(1<<20));
  msg (1, "process time %.2f seconds", seconds);
}
