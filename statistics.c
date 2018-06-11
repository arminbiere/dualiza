#include "headers.h"

Stats stats;
Rules rules;

static double average (double a, double b) { return b ? a / b : 0; }
static double percent (double a, double b) { return b ? 100*a / b : 0; }

void print_statistics () {
  if (options.verbosity < 1) return;
  msg (1, "");
  msg (1, "---- [rules] ---------------------------------------------");
  msg (1, "");
  msg (1, "%10ld EP1  %10ld EP0  %10ld EN0",
           rules.EP1, rules.EP0, rules.EN0);
  msg (1, "%10ld BP0  %10ld BP0F %10ld BP0L",
           rules.BP0, rules.BP0F,rules.BP0L);
  msg (1, "%10ld BP1F %10ld BP1L",
           rules.BP1F,rules.BP1L);
  msg (1, "%10ld BN0F %10ld BN0L",
           rules.BN0F,rules.BN0L);
#if  0
  msg (1, "%10ld AP0  %10ld AP0F %10ld AP0L",
           rules.AP0, rules.AP0F,rules.AP0L);
#endif
  msg (1, "%10ld JP0  %10ld JN0F %10ld JN0L",
           rules.JP0, rules.JN0F,rules.JN0L);
  msg (1, "%10ld DX   %10ld DY   %10ld DS",
           rules.DX,  rules.DY,  rules.DS);
  msg (1, "%10ld UP   %10ld UNX  %10ld UNY  %10ld UNT",
	   rules.UP,  rules.UNX, rules.UNY, rules.UNT);
  msg (1, "%10ld FP   %10ld FN", rules.FP, rules.FN);
  msg (1, "");
  msg (1, "---- [statistics] ----------------------------------------");
  msg (1, "");
  long resident = maximum_resident_set_size ();
  double seconds = process_time ();
  if (stats.decisions ||
      stats.conflicts.primal || stats.conflicts.dual ||
      stats.propagated.primal || stats.propagated.dual) {
    if (stats.models.counted)
      msg (1, "%ld counted models (%.1f decisions per model)",
        stats.models.counted,
	average (stats.decisions, stats.models.counted));
    if (stats.models.discounted)
      msg (1, "%ld discounted models (%.1f%% per counted model)",
        stats.models.discounted,
	percent (stats.models.discounted, stats.models.counted));
    long total = stats.conflicts.primal + stats.conflicts.dual;
    msg (1, "%ld conflicts (%.0f per second)",
      total, average (total, seconds));
    if (stats.conflicts.primal && stats.conflicts.dual)
      msg (1, "%ld primal conflicts (%.0f per second)",
	stats.conflicts.primal,
	average (stats.conflicts.primal, seconds)),
      msg (1, "%ld dual conflicts (%.0f per second)",
	stats.conflicts.dual,
	average (stats.conflicts.dual, seconds));
    if (stats.dual_shared_units)
      msg (1, "%ld dual shared units (%.0f%% dual conflicts)",
	stats.dual_shared_units,
	percent (stats.dual_shared_units, stats.conflicts.dual));
    if (stats.dual_non_shared_units)
      total = stats.dual_shared_units + stats.dual_non_shared_units,
      msg (1, "%ld dual non-shared units (%.0f%% dual units)",
	stats.dual_non_shared_units,
	percent (stats.dual_non_shared_units, total));
    msg (1, "%ld decisions (%.0f per second)",
      stats.decisions, average (stats.decisions, seconds));
    total = stats.propagated.primal + stats.propagated.dual;
    msg (1, "%ld propagations (%.1f million per second)",
      total, average (total / 1e6, seconds));
    if (stats.propagated.primal && stats.propagated.dual)
      msg (1, "%ld primal propagations (%.1f million per second)",
	stats.propagated.primal,
	average (stats.propagated.primal / 1e6, seconds)),
      msg (1, "%ld dual propagations (%.1f million per second)",
	stats.propagated.dual,
	average (stats.propagated.dual / 1e6, seconds));
    if (stats.back.tracked)
      msg (1, "%ld backtracked (%.0f%% per decision)",
      stats.back.tracked, percent (stats.back.tracked, stats.decisions));
    if (stats.back.jumped)
      msg (1, "%ld back jumped (%.0f%% per conflict)",
      stats.back.jumped,
      percent (stats.back.jumped, stats.conflicts.primal));
    if (stats.back.forced)
      msg (1, "%ld forced backtracks (%.0f%% per primal conflict)",
      stats.back.forced,
      percent (stats.back.forced, stats.conflicts.primal));
    if (stats.blocked.clauses)
      msg (1, "%ld blocking clauses (%.1f average length)",
        stats.blocked.clauses,
	average (stats.blocked.literals, stats.blocked.clauses));
    if (stats.subsumed)
      msg (1, "%ld subsumed clauses (%.1f%% of blocking clauses)",
        stats.subsumed, percent (stats.subsumed, stats.blocked.clauses));
    if (stats.restarts)
      msg (1, "%ld restarts (%.0f primal conflicts per restart)",
        stats.restarts,
        average (stats.conflicts.primal, stats.restarts));
    if (stats.reused)
      msg (1, "%ld reused (%.0f%% per restart)",
      stats.reused, percent (stats.reused, stats.restarts));
    if (stats.reductions)
      msg (1, "%ld reductions (%.0f primal conflicts per reduction)",
      stats.reductions,
      average (stats.conflicts.primal, stats.reductions));
    if (stats.collected)
      msg (1, "collected %ld clauses (%.0f per reduction)",
      stats.collected, average (stats.collected, stats.reductions));
  }
  if (stats.bumped || stats.searched) {
    msg (1, "bumped %ld variables (%.1f per primal conflict)",
      stats.bumped, average (stats.bumped,
      stats.conflicts.primal));
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
  msg (1, "");
  msg (1, "---- [resources] -----------------------------------------");
  msg (1, "");
  msg (1, "maximum allocated %ld bytes (%.1f MB)",
    stats.bytes.max, stats.bytes.max / (double)(1<<20));
  msg (1, "maximum resident set size %ld bytes (%.1f MB)",
    resident, resident / (double)(1<<20));
  msg (1, "process time %.2f seconds", seconds);
}
