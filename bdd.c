#include "headers.h"

/*------------------------------------------------------------------------*/

struct BDD {
  unsigned var, ref, hash, mark;
  unsigned long idx;
  BDD * next, * then, * other;
};

/*------------------------------------------------------------------------*/

static unsigned bdd_mark;
static BDD ** bdd_table, * false_bdd_node, * true_bdd_node;
static unsigned bdd_size, bdd_count;
static unsigned long bdd_nodes;

/*------------------------------------------------------------------------*/

long bdd_lookups, bdd_collisions;
long cache_lookups, cache_collisions;

/*------------------------------------------------------------------------*/

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
    LOG ("allocating BDD %lu var %u then %lu other %lu hash 0x%08x",
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

BDD * new_bdd (unsigned var) {
  assert (var >= 0);
  assert (true_bdd_node);
  assert (false_bdd_node);
  unsigned internal = 2 + (unsigned) var;
  return new_bdd_node (internal, true_bdd_node, false_bdd_node);
}

/*------------------------------------------------------------------------*/

static void print_bdd_recursive (BDD * b, FILE * file) {
  assert (b);
  if (b->mark == bdd_mark) return;
  if (b->idx == 0) { assert (b == false_bdd_node); return; }
  if (b->idx == 1) { assert (b == true_bdd_node); return; }
  assert (b->var > 1);
  print_bdd_recursive (b->then, file);
  print_bdd_recursive (b->other, file);
  fprintf (file,
    "%lu %u %lu %lu\n",
    b->idx, b->var-2, b->then->idx, b->other->idx);
  b->mark = bdd_mark;
}

static void inc_bdd_mark () {
  if (!++bdd_mark) die ("out of BDD marks");
}

void print_bdd_to_file (BDD * b, FILE * file) {
  inc_bdd_mark ();
  print_bdd_recursive (b, file);
}

void print_bdd (BDD * b) { print_bdd_to_file (b, stdout); }

static void visualize_bdd_recursive (BDD * b, FILE * file) {
  assert (b);
  if (b->mark == bdd_mark) return;
  b->mark = bdd_mark;
  if (b->idx <= 1) {
    fprintf (file,
      "b%lu [label=\"%u\",shape=none];\n",
      b->idx, b->var);
  } else {
    visualize_bdd_recursive (b->then, file);
    visualize_bdd_recursive (b->other, file);
    assert (b->var > 1);
    fprintf (file,
      "b%lu [label=\"%u\",shape=circle];\n",
      b->idx, b->var - 2);
    fprintf (file,
      "b%lu -> b%lu [style=solid];\n",
      b->idx, b->then->idx);
    fprintf (file,
      "b%lu -> b%lu [style=dashed];\n",
      b->idx, b->other->idx);
  }
}

#include <sys/types.h>
#include <unistd.h>

void visualize_bdd (BDD * b) {
  assert (b);
  const int path_len = 80;
  const int cmd_len = 3*path_len;
  char * base, * dot, * pdf, * cmd;
  ALLOC (base, path_len);
  ALLOC (dot, path_len);
  ALLOC (pdf, path_len);
  ALLOC (cmd, cmd_len);
  unsigned long pid = getpid ();
  sprintf (base, "/tmp/dualiza-bdd-%lu-%lu", b->idx, pid);
  sprintf (dot, "%s.dot", base);
  FILE * file = fopen (dot, "w");
  if (!file) die ("failed to open '%s'", dot);
  fputs ("digraph {\n", file);
  inc_bdd_mark ();
  visualize_bdd_recursive (b, file);
  fputs ("}\n", file);
  fclose (file);
  sprintf (pdf, "%s.pdf", base);
  sprintf (cmd, "dot -Tpdf %s > %s", dot, pdf);
  int res = system (cmd);
  msg (2, "system call returns %d", res);
  sprintf (cmd, "evince %s", pdf);
  res = system (cmd);
  msg (2, "system call returns %d", res);
  DEALLOC (cmd, cmd_len);
  unlink (dot);
  unlink (pdf);
  DEALLOC (dot, path_len);
  DEALLOC (pdf, path_len);
  DEALLOC (base, path_len);
}

/*------------------------------------------------------------------------*/

typedef struct Unary Unary;
struct Unary { BDD * a, * res; Unary * next; };

static Unary ** unary_table;
static unsigned unary_size, unary_count;

static Unary * alloc_unary (BDD * a) {
  Unary * res;
  NEW (res);
  res->a = inc (a);
  unary_count++;
  return res;
}

static void dealloc_unary (Unary * l) {
  assert (l);
  assert (unary_count);
  unary_count--;
  dec (l->a);
  dec (l->res);
  DELETE (l);
}

static unsigned hash_unary (BDD * a) { return hash_bdd_ptr (a); }

static void enlarge_unary () {
  unsigned new_unary_size = unary_size ? 2*unary_size : 1;
  msg (2, "enlarging unary cache from %u to %u", unary_size, new_unary_size);
  Unary ** new_unary_table;
  ALLOC (new_unary_table, new_unary_size);
  for (unsigned i = 0; i < unary_size; i++) {
    for (Unary * l = unary_table[i], * next; l; l = next) {
      next = l->next;
      unsigned h = hash_unary (l->a);
      h &= (new_unary_size - 1);
      l->next = new_unary_table[h];
      new_unary_table[h] = l;
    }
  }
  DEALLOC (unary_table, unary_size);
  unary_table = new_unary_table;
  unary_size = new_unary_size;
}

static Unary ** find_unary (BDD * a) {
  cache_lookups++;
  unsigned h = hash_unary (a) & (unary_size - 1);
  Unary ** res, * l;
  for (res = unary_table + h; (l = *res) && l->a != a; res = &l->next);
    cache_collisions++;
  return res;
}

static void cache_unary (BDD * a, BDD * res) {
  if (unary_count == unary_size) enlarge_unary ();
  Unary ** p = find_unary (a), * l = *p;;
  if (l) { assert (l->res == res); return; }
  *p = l = alloc_unary (a);
  l->res = inc (res);
}

static BDD * cached_unary (BDD * a) {
  if (!unary_count) return 0;
  Unary * l = *find_unary (a);
  return l ? inc (l->res) : 0;
}

static void init_unary () {
  assert (!unary_table);
  assert (!unary_size);
  assert (!unary_count);
}

static void reset_unary () {
#ifndef LOG
  unsigned lines = unary_count, old_bdd_count = bdd_count;
#endif
  for (unsigned i = 0; i < unary_size; i++)
    for (Unary * l = unary_table[i], * next; l; l = next)
      next = l->next, dealloc_unary (l);
  assert (!unary_count);
  DEALLOC (unary_table, unary_size);
  unary_size = 0;
  unary_table = 0;
#ifndef LOG
  assert (old_bdd_count >= bdd_count);
  LOG ("deleted %u BDD nodes referenced in %u unary cache entries",
    lines, old_bdd_count - bdd_count);
#endif
}

static BDD * not_bdd_recursive (BDD * a) {
  if (a == false_bdd_node) return inc (true_bdd_node);
  if (a == true_bdd_node) return inc (false_bdd_node);
  BDD * res = cached_unary (a);
  if (res) return res;
  BDD * then = not_bdd_recursive (a->then);
  BDD * other = not_bdd_recursive (a->other);
  res = new_bdd_node (a->var, then, other);
  cache_unary (a, res);
  dec (other);
  dec (then);
  return res;
}

BDD * not_bdd (BDD * a) {
  init_unary ();
  BDD * res = not_bdd_recursive (a);
  reset_unary ();
  return res;
}

/*------------------------------------------------------------------------*/

typedef struct Binary Binary;
struct Binary { BDD * a, * b, * res; Binary * next; };

static Binary ** binary_table;
static unsigned binary_size, binary_count;

static Binary * alloc_binary (BDD * a, BDD * b) {
  Binary * res;
  NEW (res);
  res->a = inc (a);
  res->b = inc (b);
  binary_count++;
  return res;
}

static void dealloc_binary (Binary * l) {
  assert (l);
  assert (binary_count);
  binary_count--;
  dec (l->a);
  dec (l->b);
  dec (l->res);
  DELETE (l);
}

static unsigned hash_binary (BDD * a, BDD * b) {
  return hash_bdd_ptr (a) * primes[0] + hash_bdd_ptr (b);
}

static void enlarge_binary () {
  unsigned new_binary_size = binary_size ? 2*binary_size : 1;
  msg (2, "enlarging binary cache from %u to %u", binary_size, new_binary_size);
  Binary ** new_binary_table;
  ALLOC (new_binary_table, new_binary_size);
  for (unsigned i = 0; i < binary_size; i++) {
    for (Binary * l = binary_table[i], * next; l; l = next) {
      next = l->next;
      unsigned h = hash_binary (l->a, l->b);
      h &= (new_binary_size - 1);
      l->next = new_binary_table[h];
      new_binary_table[h] = l;
    }
  }
  DEALLOC (binary_table, binary_size);
  binary_table = new_binary_table;
  binary_size = new_binary_size;
}

static Binary ** find_binary (BDD * a, BDD * b) {
  cache_lookups++;
  unsigned h = hash_binary (a, b) & (binary_size - 1);
  Binary ** res, * l;
  for (res = binary_table + h;
       (l = *res) && (l->a != a || l->b != b);
       res = &l->next);
    cache_collisions++;
  return res;
}

static void cache_binary (BDD * a, BDD * b, BDD * res) {
  if (binary_count == binary_size) enlarge_binary ();
  Binary ** p = find_binary (a, b), * l = *p;;
  if (l) { assert (l->res == res); return; }
  *p = l = alloc_binary (a, b);
  l->res = inc (res);
}

static BDD * cached_binary (BDD * a, BDD * b) {
  if (!binary_count) return 0;
  Binary * l = *find_binary (a, b);
  return l ? inc (l->res) : 0;
}

static void init_binary () {
  assert (!binary_table);
  assert (!binary_size);
  assert (!binary_count);
}

static void reset_binary () {
#ifndef LOG
  unsigned lines = binary_count, old_bdd_count = bdd_count;
#endif
  for (unsigned i = 0; i < binary_size; i++)
    for (Binary * l = binary_table[i], * next; l; l = next)
      next = l->next, dealloc_binary (l);
  assert (!binary_count);
  DEALLOC (binary_table, binary_size);
  binary_size = 0;
  binary_table = 0;
#ifndef LOG
  assert (old_bdd_count >= bdd_count);
  LOG ("deleted %u BDD nodes referenced in %u binary cache entries",
    lines, old_bdd_count - bdd_count);
#endif
}

static BDD * and_bdd_recursive (BDD * a, BDD * b) {
  if (a == false_bdd_node || b == false_bdd_node)
    return inc (false_bdd_node);
  if (a == true_bdd_node || a == b)
    return inc (b);
  if (b == true_bdd_node)
    return inc (a);
  if (a->idx > b->idx) SWAP (BDD*, a, b);
  BDD * res = cached_binary (a, b);
  if (res) return res;
  unsigned var = MAX (a->var, b->var);
  BDD * a_then = a->var == var ? a->then : a;
  BDD * b_then = b->var == var ? b->then : b;
  BDD * then = and_bdd_recursive (a_then, b_then);
  BDD * a_other = a->var == var ? a->other : a;
  BDD * b_other = b->var == var ? b->other : b;
  BDD * other = and_bdd_recursive (a_other, b_other);
  res = new_bdd_node (var, then, other);
  cache_binary (a, b, res);
  dec (other);
  dec (then);
  return res;
}

static BDD * xor_bdd_recursive (BDD * a, BDD * b) {
  if (a == false_bdd_node) return inc (b);
  if (b == false_bdd_node) return inc (a);
  if (a == b) return inc (false_bdd_node);
  if (a->idx > b->idx) SWAP (BDD*, a, b);
  BDD * res = cached_binary (a, b);
  if (res) return res;
  unsigned var = MAX (a->var, b->var);
  BDD * a_then = a->var == var ? a->then : a;
  BDD * b_then = b->var == var ? b->then : b;
  BDD * then = xor_bdd_recursive (a_then, b_then);
  BDD * a_other = a->var == var ? a->other : a;
  BDD * b_other = b->var == var ? b->other : b;
  BDD * other = xor_bdd_recursive (a_other, b_other);
  res = new_bdd_node (var, then, other);
  cache_binary (a, b, res);
  dec (other);
  dec (then);
  return res;
}

static BDD * or_bdd_recursive (BDD * a, BDD * b) {
  if (a == true_bdd_node || b == true_bdd_node)
    return inc (true_bdd_node);
  if (a == false_bdd_node || a == b)
    return inc (b);
  if (b == false_bdd_node)
    return inc (a);
  if (a->idx > b->idx) SWAP (BDD*, a, b);
  BDD * res = cached_binary (a, b);
  if (res) return res;
  unsigned var = MAX (a->var, b->var);
  BDD * a_then = a->var == var ? a->then : a;
  BDD * b_then = b->var == var ? b->then : b;
  BDD * then = or_bdd_recursive (a_then, b_then);
  BDD * a_other = a->var == var ? a->other : a;
  BDD * b_other = b->var == var ? b->other : b;
  BDD * other = or_bdd_recursive (a_other, b_other);
  res = new_bdd_node (var, then, other);
  cache_binary (a, b, res);
  dec (other);
  dec (then);
  return res;
}

static BDD * xnor_bdd_recursive (BDD * a, BDD * b) {
  if (a == true_bdd_node) return inc (b);
  if (b == true_bdd_node) return inc (a);
  if (a == b) return inc (true_bdd_node);
  if (a->idx > b->idx) SWAP (BDD*, a, b);
  BDD * res = cached_binary (a, b);
  if (res) return res;
  unsigned var = MAX (a->var, b->var);
  BDD * a_then = a->var == var ? a->then : a;
  BDD * b_then = b->var == var ? b->then : b;
  BDD * then = xnor_bdd_recursive (a_then, b_then);
  BDD * a_other = a->var == var ? a->other : a;
  BDD * b_other = b->var == var ? b->other : b;
  BDD * other = xnor_bdd_recursive (a_other, b_other);
  res = new_bdd_node (var, then, other);
  cache_binary (a, b, res);
  dec (other);
  dec (then);
  return res;
}

BDD * and_bdd (BDD * a, BDD * b) {
  init_binary ();
  BDD * res = and_bdd_recursive (a, b);
  reset_binary ();
  return res;
}

BDD * xor_bdd (BDD * a, BDD * b) {
  init_binary ();
  BDD * res = xor_bdd_recursive (a, b);
  reset_binary ();
  return res;
}

BDD * or_bdd (BDD * a, BDD * b) {
  init_binary ();
  BDD * res = or_bdd_recursive (a, b);
  reset_binary ();
  return res;
}

BDD * xnor_bdd (BDD * a, BDD * b) {
  init_binary ();
  BDD * res = xnor_bdd_recursive (a, b);
  reset_binary ();
  return res;
}

/*------------------------------------------------------------------------*/

typedef struct Ternary Ternary;
struct Ternary { BDD * a, * b, * c, * res; Ternary * next; };

static Ternary ** ternary_table;
static unsigned ternary_size, ternary_count;

static Ternary * alloc_ternary (BDD * a, BDD * b, BDD * c) {
  Ternary * res;
  NEW (res);
  res->a = inc (a);
  res->b = inc (b);
  res->c = inc (c);
  ternary_count++;
  return res;
}

static void dealloc_ternary (Ternary * l) {
  assert (l);
  assert (ternary_count);
  ternary_count--;
  dec (l->a);
  dec (l->b);
  dec (l->c);
  dec (l->res);
  DELETE (l);
}

static unsigned hash_ternary (BDD * a, BDD * b, BDD * c) {
  unsigned res = hash_bdd_ptr (a) * primes[0];
  res = res*primes[0] + hash_bdd_ptr (b);
  res = res*primes[1] + hash_bdd_ptr (c);
  return res;
}

static void enlarge_ternary () {
  unsigned new_ternary_size = ternary_size ? 2*ternary_size : 1;
  msg (2, "enlarging ternary cache from %u to %u", ternary_size, new_ternary_size);
  Ternary ** new_ternary_table;
  ALLOC (new_ternary_table, new_ternary_size);
  for (unsigned i = 0; i < ternary_size; i++) {
    for (Ternary * l = ternary_table[i], * next; l; l = next) {
      next = l->next;
      unsigned h = hash_ternary (l->a, l->b, l->c);
      h &= (new_ternary_size - 1);
      l->next = new_ternary_table[h];
      new_ternary_table[h] = l;
    }
  }
  DEALLOC (ternary_table, ternary_size);
  ternary_table = new_ternary_table;
  ternary_size = new_ternary_size;
}

static Ternary ** find_ternary (BDD * a, BDD * b, BDD * c) {
  cache_lookups++;
  unsigned h = hash_ternary (a, b, c) & (ternary_size - 1);
  Ternary ** res, * l;
  for (res = ternary_table + h;
       (l = *res) && (l->a != a || l->b != b || l->c != c);
       res = &l->next);
    cache_collisions++;
  return res;
}

static void cache_ternary (BDD * a, BDD * b, BDD * c, BDD * res) {
  if (ternary_count == ternary_size) enlarge_ternary ();
  Ternary ** p = find_ternary (a, b, c), * l = *p;;
  if (l) { assert (l->res == res); return; }
  *p = l = alloc_ternary (a, b, c);
  l->res = inc (res);
}

static BDD * cached_ternary (BDD * a, BDD * b, BDD * c) {
  if (!ternary_count) return 0;
  Ternary * l = *find_ternary (a, b, c);
  return l ? inc (l->res) : 0;
}

static void init_ternary () {
  assert (!ternary_table);
  assert (!ternary_size);
  assert (!ternary_count);
}

static void reset_ternary () {
#ifndef LOG
  unsigned lines = ternary_count, old_bdd_count = bdd_count;
#endif
  for (unsigned i = 0; i < ternary_size; i++)
    for (Ternary * l = ternary_table[i], * next; l; l = next)
      next = l->next, dealloc_ternary (l);
  assert (!ternary_count);
  DEALLOC (ternary_table, ternary_size);
  ternary_size = 0;
  ternary_table = 0;
#ifndef LOG
  assert (old_bdd_count >= bdd_count);
  LOG ("deleted %u BDD nodes referenced in %u ternary cache entries",
    lines, old_bdd_count - bdd_count);
#endif
}

static BDD * ite_bdd_recursive (BDD * a, BDD * b, BDD * c) {
  if (a == true_bdd_node) return inc (b);
  if (a == false_bdd_node) return inc (c);
  if (b == c) return inc (b);
  // a ? a : c == a&a | !a&c = a | !a & c == a | c
  // a ? 1 : c == a&1 | !a&c = a | !a & c == a | c
  if (a == b || b == true_bdd_node) {
    if (a == true_bdd_node || c == true_bdd_node)
      return inc (true_bdd_node);
    if (a == false_bdd_node || a == c)
      return inc (c);
    if (c == false_bdd_node)
      return inc (a);
  }
  // a ? b : a == a&b | !a&a = a & b | 0 == a | b
  // a ? b : 0 == a&b | !a&0 = a & b | 0 == a | b
  if (a == c || c == false_bdd_node) {
    if (a == false_bdd_node || b == false_bdd_node)
      return inc (false_bdd_node);
    if (a == b)
      return inc (b);
    if (b == true_bdd_node)
      return inc (a);
    }
  BDD * res = cached_ternary (a, b, c);
  if (res) return res;
  unsigned var = MAX (b->var, c->var);
  if (var < a->var) var = a->var;
  BDD * a_then = a->var == var ? a->then : a;
  BDD * b_then = b->var == var ? b->then : b;
  BDD * c_then = c->var == var ? c->then : c;
  BDD * then = ite_bdd_recursive (a_then, b_then, c_then);
  BDD * a_other = a->var == var ? a->other : a;
  BDD * b_other = b->var == var ? b->other : b;
  BDD * c_other = c->var == var ? c->other : c;
  BDD * other = ite_bdd_recursive (a_other, b_other, c_other);
  res = new_bdd_node (var, then, other);
  cache_ternary (a, b, c, res);
  dec (other);
  dec (then);
  return res;
}

BDD * ite_bdd (BDD * a, BDD * b, BDD * c) {
  init_ternary ();
  BDD * res = ite_bdd_recursive (a, b, c);
  reset_ternary ();
  return res;
}

/*------------------------------------------------------------------------*/

typedef struct Count Count;
struct Count { BDD * a; Count * next; Number res; };

static Count ** count_table;
static unsigned count_size, count_count;

static Count * alloc_count (BDD * a) {
  Count * res;
  NEW (res);
  res->a = inc (a);
  init_number (res->res);
  count_count++;
  return res;
}

static void dealloc_count (Count * l) {
  assert (l);
  assert (count_count);
  count_count--;
  dec (l->a);
  clear_number (l->res);
  DELETE (l);
}

static unsigned hash_count (BDD * a) { return hash_bdd_ptr (a); }

static void enlarge_count () {
  unsigned new_count_size = count_size ? 2*count_size : 1;
  msg (2, "enlarging count cache from %u to %u", count_size, new_count_size);
  Count ** new_count_table;
  ALLOC (new_count_table, new_count_size);
  for (unsigned i = 0; i < count_size; i++) {
    for (Count * l = count_table[i], * next; l; l = next) {
      next = l->next;
      unsigned h = hash_count (l->a);
      h &= (new_count_size - 1);
      l->next = new_count_table[h];
      new_count_table[h] = l;
    }
  }
  DEALLOC (count_table, count_size);
  count_table = new_count_table;
  count_size = new_count_size;
}

static Count ** find_count (BDD * a) {
  cache_lookups++;
  unsigned h = hash_count (a) & (count_size - 1);
  Count ** res, * l;
  for (res = count_table + h; (l = *res) && l->a != a; res = &l->next);
    cache_collisions++;
  return res;
}

static void cache_count (BDD * a, const Number res) {
  if (count_count == count_size) enlarge_count ();
  Count ** p = find_count (a), * l = *p;;
  if (l) return;
  *p = l = alloc_count (a);
  copy_number (l->res, res);
}

static int cached_count (Number res, BDD * a) {
  if (!count_count) return 0;
  Count * l = *find_count (a);
  if (!l) return 0;
  copy_number (res, l->res);
  return 1;
}

static void init_count () {
  assert (!count_table);
  assert (!count_size);
  assert (!count_count);
}

static void reset_count () {
#ifndef LOG
  unsigned lines = count_count, old_bdd_count = bdd_count;
#endif
  for (unsigned i = 0; i < count_size; i++)
    for (Count * l = count_table[i], * next; l; l = next)
      next = l->next, dealloc_count (l);
  assert (!count_count);
  DEALLOC (count_table, count_size);
  count_size = 0;
  count_table = 0;
#ifndef LOG
  assert (old_bdd_count >= bdd_count);
  LOG ("deleted %u BDD nodes referenced in %u count cache entries",
    lines, old_bdd_count - bdd_count);
#endif
}

static void count_bdd_recursive (Number res, BDD * a, unsigned max_var) {
  assert (a);
  assert (max_var >= 1);
  assert (a->var <= max_var);
  if (a == false_bdd_node) return;
  if (a == true_bdd_node) {
    assert (is_zero_number (res));
    add_power_of_two_to_number (res, max_var - 1);
    return;
  }
  if (cached_count (res, a)) return;
  Number tmp;
  init_number (tmp);
  count_bdd_recursive (tmp, a->then, a->var - 1);
  count_bdd_recursive (res, a->other, a->var - 1);
  add_number (res, tmp);
  clear_number (tmp);
  multiply_number_by_power_of_two (res, max_var - a->var);
  cache_count (a, res);
}

void count_bdd (Number res, BDD * b, unsigned max_var) {
  assert (b);
  assert (b->var <= max_var + 2);
  init_count ();
  count_bdd_recursive (res, b, max_var + 2);
  reset_count ();
}
