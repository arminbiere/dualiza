#include "headers.h"

Symbols * new_symbols () {
  Symbols * res;
  NEW (res);
  return res;
}

static void delete_symbol (Symbol * s) {
  STRDEL (s->name);
  DELETE (s);
}

void delete_symbols (Symbols * t) {
  for (unsigned i = 0; i < t->size; i++) {
    Symbol * next;
    for (Symbol * s = t->table[i]; s; s = next) {
      next = s->next;
      delete_symbol (s);
    }
  }
  DEALLOC (t->table, t->size);
  DELETE (t);
}

static Symbol * new_symbol (const char * name) {
  Symbol * res;
  NEW (res);
  STRDUP (res->name, name);
  LOG ("new symbol '%s'", name);
  return res;
}

static unsigned hash_string (const char * name) {
  unsigned res = 0, i = 0;
  for (const char * p = name; *p; p++) {
    unsigned char ch = *p;
    res += ch | (ch << 8) | (ch << 16) | (ch << 24);
    if (i == num_primes) i = 0;
    res *= primes[i++];
  }
  return res;
}

static Symbol ** lookup_symbol (Symbols * t, const char * name) {
  stats.symbol.lookups++;
  unsigned h = hash_string (name) & (t->size - 1);
  Symbol ** p, * s;
  for (p = t->table + h; (s = *p) && strcmp (name, s->name); p = &s->next)
    stats.symbol.collisions++;
  return p;
}

static void enlarge_symbols (Symbols * t) {
  unsigned old_size = t->size, new_size = old_size ? 2*old_size : 1;
  Symbol ** old_table = t->table, ** new_table;
  ALLOC (new_table, new_size);
  for (unsigned i = 0; i < old_size; i++) {
    Symbol * next;
    for (Symbol * s = old_table[i]; s; s = next) {
      unsigned h = hash_string (s->name) & (new_size - 1);
      next = s->next;
      s->next = new_table[h];
      new_table[h] = s;
    }
  }
  DEALLOC (old_table, old_size);
  t->size = new_size;
  t->table = new_table;
  msg (2, "enlarged symbol table from %u to %u", old_size, new_size); 
}

Symbol * find_or_create_symbol (Symbols * t, const char * name) {
  if (t->size == t->count) enlarge_symbols (t);
  Symbol ** p = lookup_symbol (t, name), * s = *p;
  if (!s) *p = s = new_symbol (name), t->count++;
  return s;
}
