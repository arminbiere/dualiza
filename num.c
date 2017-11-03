#include "headers.h"

#ifdef GMP

void init_number (Number n) { mpz_init (n); }

void copy_number (Number dst, Number src) {
}

void clear_number (Number n) { mpz_clear (n); }

void multiply_number_by_power_of_two (Number n, Exponent e) {
  mpz_mul_2exp (n, n, e);
}
void add_power_of_two_to_number (Number n, Exponent e) {
  mpz_t tmp;
  mpz_init_set_ui (tmp, 1);
  mpz_mul_2exp (tmp, tmp, e);
  mpz_add (n, n, tmp);
  mpz_clear (tmp);
}

void print_number_to_file (Number n, FILE * file) {
  mpz_out_str (file, 10, n);
}

#else

void init_number (Number n) { INIT (*n); }

void clear_number (Number n) { RELEASE (n[0]); }

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
      unsigned old_t = q[j], old_r = r, t;
      assert (old_r < 10);
#if 1
      unsigned long tl = old_t, rl;
      tl += ((unsigned long) old_r) << 32;
      rl = tl % 10;
      tl = tl / 10;
      assert (tl <= (unsigned long) UINT_MAX);
      r = rl, t = tl;
#endif
#if 0
      unsigned tu = old_t, ru = tu % 10;
      tu = tu / 10;
      tu += old_r * 429496729;
      ru += old_r * 6;
      tu += ru/10;
      ru %= 10;
      r = ru, t = tu;
#endif
#if 0
      assert (tl == tu);
      assert (rl == ru);
#endif
      assert (r < 10);
      q[j] = t;
    }
    assert (r < 10);
    unsigned char c = r + '0';
    PUSH (*s, c);
  }
  DEALLOC (q, k);
}

static void normalize_number (Number n) {
  while (!EMPTY (n[0]) && !TOP (n[0]))
    (void) POP (n[0]);
}

void multiply_number_by_power_of_two (Number n, Exponent e) {
  const unsigned long old_count = COUNT (n[0]);
  if (!old_count) return;
  const unsigned long word = e >> 5;
  const unsigned bit = e & 31;
  const unsigned long new_count = old_count + word + (bit != 0);
  while (new_count > SIZE (n[0])) ENLARGE (n[0]);
  n[0].top = n[0].start + new_count;
  unsigned src = old_count, dst = new_count;
  unsigned * start = n[0].start;
  if (bit) start[--dst] = start[src-1] >> (32 - bit);
  while (src) {
    const unsigned data = start[--src];
    const unsigned prev = bit && src ? start[src-1] : 0;
    start[--dst] = (data << bit) | (prev >> (32 - bit));
  }
  while (dst) start[--dst] = 0;
  normalize_number (n);
}

void add_power_of_two_to_number (Number n, Exponent e) {
  assert (n);
  unsigned long word = e >> 5;
  const unsigned bit = e & 31;
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

void sub_power_of_two_from_number (Number n, Exponent e) {
  assert (n);
  unsigned long word = e >> 5;
  const unsigned bit = e & 31;
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
  normalize_number (n);
}

void print_number_to_file (Number n, FILE * file) {
  CharStack stack;
  INIT (stack);
  print_number_to_stack (n, &stack);
  while (!EMPTY (stack))
    fputc (POP (stack), stdout);
  RELEASE (stack);
}

#endif

void println_number_to_file (Number n, FILE * file) {
  print_number_to_file (n, file);
  fputc ('\n', file);
}

void println_number (Number n) {
  println_number_to_file (n, stdout);
}
