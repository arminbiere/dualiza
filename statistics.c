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
    msg (1, "%ld propagations (%.1f million per second)",
      stats.propagated, average (stats.propagated / 1e6, seconds));
    if (stats.back.tracked)
      msg (1, "%ld backtracked (%.0f%% per decision)",
      stats.back.tracked, percent (stats.back.tracked, stats.decisions));
    if (stats.back.jumped)
      msg (1, "%ld back jumped (%.0f%% per conflict)",
      stats.back.jumped, percent (stats.back.jumped, stats.conflicts));
    if (stats.back.forced)
      msg (1, "%ld forced backtracks (%.0f%% per conflict)",
      stats.back.forced, percent (stats.back.forced, stats.conflicts));
    long blocked = stats.blocked.eager.clauses + stats.blocked.lazy.clauses;
    if (stats.blocked.eager.clauses)
      msg (1, "%ld eager blocking clauses (%.1f%% of %.1f average length)",
        stats.blocked.eager.clauses,
	percent (stats.blocked.eager.clauses, blocked),
	average (stats.blocked.eager.literals, stats.blocked.eager.clauses));
    if (stats.blocked.lazy.clauses)
      msg (1, "%ld lazy blocking clauses (%.1f%% of %.1f average length)",
        stats.blocked.lazy.clauses,
	percent (stats.blocked.lazy.clauses, blocked),
	average (stats.blocked.lazy.literals, stats.blocked.lazy.clauses));
    if (stats.subsumed)
      msg (1, "%ld subsumed clauses (%.1f%% of blocked clauses)",
        stats.subsumed, percent (stats.subsumed, blocked));
    if (stats.rephased)
      msg (1, "%ld rephased (%.0f conflicts per rephase)",
      stats.rephased, average (stats.conflicts, stats.rephased));
    if (stats.restarts)
      msg (1, "%ld restarts (%.0f conflicts per restart)",
      stats.restarts, average (stats.conflicts, stats.restarts));
    if (stats.reused)
      msg (1, "%ld reused (%.0f%% per restart)",
      stats.reused, percent (stats.reused, stats.restarts));
    if (stats.reductions)
      msg (1, "%ld reductions (%.0f conflicts per reduction)",
      stats.reductions, average (stats.conflicts, stats.reductions));
    if (stats.collected)
      msg (1, "collected %ld clauses (%.0f per reduction)",
      stats.collected, average (stats.collected, stats.reductions));
  }
  if (stats.bumped || stats.searched) {
    msg (1, "bumped %ld variables (%.1f per conflict)",
      stats.bumped, average (stats.bumped, stats.conflicts));
    msg (1, "searched %ld variables (%.1f per decision)",
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
