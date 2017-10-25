#include "headers.h"

struct BDD {
  unsigned var, ref, hash, mark;
  unsigned long idx;
  BDD * next, * then, * other;
};

typedef struct Line Line;
struct Line { BDD * a, * b, * c, * res; Line * next; Number n; };

static BDD ** bdd_table, * false_bdd_node, * true_bdd_node;
static unsigned bdd_size, bdd_count;
static unsigned long bdd_nodes;

long bdd_lookups, bdd_collisions;
long cache_lookups, cache_collisions;

static Line ** cache_table;
static unsigned cache_size, cache_count;

static BDD * inc (BDD * b) {
  assert (b);
  assert (b->ref);
  b->ref++;
  return b;
}

static BDD *
alloc_bdd (unsigned var, BDD * then, BDD * other, unsigned hash) {
  assert (!then == !other);
  BDD * res;
  NEW (res);
  res->var = var;
  res->ref = 1;
  res->hash = hash;
  res->idx = bdd_nodes++;
  res->then = then ? inc (then) : 0;
  res->other = other ? inc (other) : 0;
  bdd_count++;
#ifndef NLOG
  if (then) {
    LOG ("allocating BDD %lu var %d then %lu other %lu hash 0x%08x",
      res->idx, var, then->idx, other->idx, hash);
  } else
    LOG ("allocating BDD %lu %s hash 0x%08x",
      res->idx, (var ? "true" : "false"), hash);
#endif
  return res;
}

static void dealloc_bdd (BDD * b) {
  assert (b);
  LOG ("deallocating BDD %lu", b->idx);
  assert (bdd_count);
  bdd_count--;
  DELETE (b);
}

static BDD **
find_bdd (unsigned var, BDD * then, BDD * other, unsigned hash) {
  bdd_lookups++;
  unsigned h = hash & (bdd_size - 1);
  BDD ** res, * b;
  for (res = bdd_table + h;
       (b = *res) &&
         (b->var != var || b->then != then || b->other != other);
      res = &b->next)
    bdd_collisions++;
  return res;
}

static void dec (BDD * b) {
  assert (b->ref);
  if (--b->ref) return;
  BDD ** p = find_bdd (b->var, b->then, b->other, b->hash);
  assert (*p == b);
  *p = b->next;
  if (b->then) delete_bdd (b->then);
  if (b->other) delete_bdd (b->other);
  dealloc_bdd (b);
}

BDD * copy_bdd (BDD * b) { return inc (b); }

void delete_bdd (BDD * b) { dec (b); }

static unsigned hash_bdd_ptr (BDD * b) { return b ? b->hash : 0; }

static unsigned hash_bdd (unsigned var, BDD * then, BDD * other) {
  assert (!(num_primes & (num_primes - 1)));
  unsigned i = var & (num_primes - 1);
  unsigned res = var * primes[i++];
  if (i == num_primes) i = 0;
  res = (res + hash_bdd_ptr (then)) * primes[i++];
  if (i == num_primes) i = 0;
  res = (res + hash_bdd_ptr (other)) * primes[i];
  return res;
}

static void enlarge_bdd () {
  unsigned new_bdd_size = bdd_size ? 2*bdd_size : 1;
  msg (2, "enlarging BDD table from %u to %u", bdd_size, new_bdd_size);
  BDD ** new_bdd_table;
  ALLOC (new_bdd_table, new_bdd_size);
  for (unsigned i = 0; i < bdd_size; i++) {
    for (BDD * b = bdd_table[i], * next; b; b = next) {
      next = b->next;
      unsigned h = b->hash & (new_bdd_size - 1);
      b->next = new_bdd_table[h];
      new_bdd_table[h] = b;
    }
  }
  DEALLOC (bdd_table, bdd_size);
  bdd_table = new_bdd_table;
  bdd_size = new_bdd_size;
}

static BDD * new_bdd_node (unsigned var, BDD * then, BDD * other) {
  if (bdd_size == bdd_count) enlarge_bdd ();
  unsigned hash = hash_bdd (var, then, other);
  BDD ** p = find_bdd (var, then, other, hash), * res;
  if ((res = *p)) return copy_bdd (res);
  *p = res = alloc_bdd (var, then, other, hash);
  return res;
}

BDD * false_bdd () {
  assert (false_bdd_node);
  return copy_bdd (false_bdd_node);
}

BDD * true_bdd () {
  assert (true_bdd_node);
  return copy_bdd (true_bdd_node);
}

void init_bdds () {
  false_bdd_node = new_bdd_node (0, 0, 0);
  true_bdd_node = new_bdd_node (1, 0, 0);
}

void reset_bdds () {
  for (unsigned i = 0; i < bdd_size; i++)
    for (BDD * b = bdd_table[i], * next; b; b = next)
      next = b->next, dealloc_bdd (b);
  assert (!bdd_count);
  DEALLOC (bdd_table, bdd_size);
  false_bdd_node = 0;
  true_bdd_node = 0;
}

BDD * new_bdd (int var) {
  assert (var >= 0);
  assert (true_bdd_node);
  assert (false_bdd_node);
  unsigned internal = 2 + (unsigned) var;
  return new_bdd_node (internal, true_bdd_node, false_bdd_node);
}

static Line * alloc_line (BDD * a, BDD * b, BDD * c) {
  Line * res;
  NEW (res);
  res->a = inc (a);
  res->b = b ? inc (b) : 0;
  res->c = c ? inc (c) : 0;
  cache_count++;
  return res;
}

static void dealloc_line (Line * l) {
  assert (l);
  assert (cache_count);
  cache_count--;
  dec (l->a);
  if (l->b) dec (l->b);
  if (l->c) dec (l->c);
  if (l->res) dec (l->res);
  else clear_number (l->n);
  DELETE (l);
}

static unsigned hash_cache (BDD * a, BDD * b, BDD * c) {
  unsigned res = hash_bdd_ptr (a);
  res += hash_bdd_ptr (b);
  res += hash_bdd_ptr (c);
  return res;
}

static void enlarge_cache () {
  unsigned new_cache_size = cache_size ? 2*cache_size : 1;
  msg (2, "enlarging cache from %u to %u", cache_size, new_cache_size);
  Line ** new_cache_table;
  ALLOC (new_cache_table, new_cache_size);
  for (unsigned i = 0; i < cache_size; i++) {
    for (Line * l = cache_table[i], * next; l; l = next) {
      next = l->next;
      unsigned h = hash_cache (l->a, l->b, l->c);
      h &= (new_cache_size - 1);
      l->next = new_cache_table[h];
      new_cache_table[h] = l;
    }
  }
  DEALLOC (cache_table, cache_size);
  cache_table = new_cache_table;
  cache_size = new_cache_size;
}

static Line ** find_cache (BDD * a, BDD * b, BDD * c) {
  cache_lookups++;
  unsigned h = hash_cache (a, b, c) & (cache_size - 1);
  Line ** res, * l;
  for (res = cache_table + h;
       (l = *res) && (l->a != a || l->b != b || l->c != c);
       res = &l->next);
    cache_collisions++;
  return res;
}

static void insert_cache (BDD * arg0, BDD * res) {
  if (cache_count == cache_size) enlarge_cache ();
}

static void init_cache () {
  assert (!cache_table);
  assert (!cache_size);
  assert (!cache_count);
}

static void reset_cache () {
#ifndef LOG
  unsigned lines = cache_count, old_bdd_count = bdd_count;
#endif
  for (unsigned i = 0; i < cache_size; i++)
    for (Line * l = cache_table[i], * next; l; l = next)
      next = l->next, dealloc_line (l);
  assert (!cache_count);
  DEALLOC (cache_table, cache_size);
  cache_size = 0;
  cache_table = 0;
#ifndef LOG
  assert (old_bdd_count >= bdd_count);
  LOG ("deleted %u BDD nodes referenced in %u cache entries",
    lines, old_bdd_count - bdd_count);
#endif
}

static void unmark_bdd (BDD * b) {
  assert (b);
  if (!b->mark) return;
  b->mark = 0;
  if (b->then) unmark_bdd (b->then);
  if (b->other) unmark_bdd (b->other);
}

static void print_bdd_recursive (BDD * b, FILE * file) {
  assert (b);
  if (b->mark) return;
  if (b->idx == 0) { assert (b == false_bdd_node); return; }
  if (b->idx == 1) { assert (b == true_bdd_node); return; }
  assert (b->var > 1);
  print_bdd_recursive (b->then, file);
  print_bdd_recursive (b->other, file);
  fprintf (file,
    "%lu %u %lu %lu\n",
    b->idx, b->var-2, b->then->idx, b->other->idx);
  b->mark = 1;
}

void print_bdd_to_file (BDD * b, FILE * file) {
  print_bdd_recursive (b, file);
  unmark_bdd (b);
}

void print_bdd (BDD * b) { print_bdd_to_file (b, stdout); }
