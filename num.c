#include "headers.h"

Number * new_number () {
  Number * res;
  NEW (res);
  return res;
}

void init_number (Number n) {
  INIT (*n);
}

void reset_number (Number n) {
  RELEASE (n[0]);
}

void print_number_as_sum_of_powers_of_two_to_file (Number n,
                                                   FILE * file) {
  const int size = COUNT (n[0]);
  int non_zero = 0;
  for (int i = size-1; i >= 0; i--) {
    unsigned m = n[0].start[i];
    if (!m) continue;
    if (non_zero++) fputs (" + ", file);
    fprintf (file, "%u", m);
    if (i) fprintf (file, "*2^%d", 32*i);
  }
  if (!non_zero) fputc ('0', file);
}

static void print_number_to_stack (Number n, CharStack * s) {
  int k = COUNT (n[0]);
  if (!k) { PUSH (*s, '0'); return; }
  unsigned * q;
  ALLOC (q, k);
  memcpy (q, n[0].start, k * sizeof (unsigned));
  int i = k-1;
  for (;;) {
    unsigned r = 0;
    while (i >= 0 && !q[i])
      i--;
    if (i < 0) break;
    for (int j = i; j >= 0; j--) {
      unsigned long t = q[j];
      assert (r < 10);
      t += ((unsigned long) r) << 32;
      r = t % 10;
      t = t / 10;
      assert (t <= (unsigned long) UINT_MAX);
      q[j] = t;
    }
    assert (r < 10);
    unsigned char c = r + '0';
    PUSH (*s, c);
  }
  DEALLOC (q, k);
}

void print_number_to_file (Number n, FILE * file) {
  CharStack stack;
  INIT (stack);
  print_number_to_stack (n, &stack);
  while (!EMPTY (stack))
    fputc (POP (stack), stdout);
  RELEASE (stack);
}

void println_number_to_file (Number n, FILE * file) {
  print_number_to_file (n, file);
  fputc ('\n', file);
}

void println_number (Number n) {
  println_number_to_file (n, stdout);
}

void add_power_of_two_to_number (Number n, int e) {
  assert (n);
  assert (e >= 0);
  long word = e >> 5;
  const int bit = e & 31;
  unsigned inc = 1u << bit;
  for (;;) {
    if (word < COUNT (n[0])) {
      unsigned before = n[0].start[word];
      unsigned after = before + inc;
      n[0].start[word] = after;
      if (after) break;
      inc = 1, word++;
    } else PUSH (n[0], 0);
  }
}

void sub_power_of_two_from_number (Number n, int e) {
  assert (n);
  assert (e >= 0);
  long word = e >> 5;
  const int bit = e & 31;
  unsigned dec = 1u << bit;
  unsigned * words = n[0].start;
  while (dec) {
    assert (word < COUNT (n[0]));
    unsigned before = words[word];
    unsigned after = before - dec;
    words[word] = after;
    if (after < before) break;
    dec = 1, word++;
  }
  while (!EMPTY (n[0]) && !TOP (n[0]))
    (void) POP (n[0]);
}
