#include "headers.h"

#include <stdint.h>

/*------------------------------------------------------------------------*/
#ifdef GMP
/*------------------------------------------------------------------------*/

void
init_number (Number n)
{
  mpz_init (n);
}

void
init_number_from_unsigned (Number n, unsigned u)
{
  mpz_init_set_ui (n, u);
}

int
is_zero_number (Number n)
{
  return !mpz_cmp_ui (n, 0);
}

int
cmp_number (Number a, Number b)
{
  return mpz_cmp (a, b);
}

void
copy_number (Number dst, const Number src)
{
  mpz_set (dst, src);
}

void
clear_number (Number n)
{
  mpz_clear (n);
}

void
multiply_number_by_power_of_two (Number n, Exponent e)
{
  mpz_mul_2exp (n, n, e);
}

void
add_power_of_two_to_number (Number n, Exponent e)
{
  mpz_t tmp;
  mpz_init_set_ui (tmp, 1);
  mpz_mul_2exp (tmp, tmp, e);
  mpz_add (n, n, tmp);
  mpz_clear (tmp);
}

void
sub_power_of_two_from_number (Number n, Exponent e)
{
  mpz_t tmp;
  mpz_init_set_ui (tmp, 1);
  mpz_mul_2exp (tmp, tmp, e);
  mpz_sub (n, n, tmp);
  mpz_clear (tmp);
}

void
add_number (Number res, const Number other)
{
  mpz_add (res, res, other);
}

void
sub_number (Number res, const Number other)
{
  mpz_sub (res, res, other);
}

void
print_number_to_file (Number n, FILE * file)
{
  mpz_out_str (file, 10, n);
}

/*------------------------------------------------------------------------*/
#else // our own home-made multiple precision number library
/*------------------------------------------------------------------------*/

#ifndef NDEBUG
static int
is_normalized_number (Number n)
{
  return EMPTY (*n) || TOP (*n);
}
#endif

void
init_number (Number n)
{
  INIT (*n);
}

void
init_number_from_unsigned (Number n, unsigned u)
{
  INIT (*n);
  if (n)
    PUSH (*n, u);
}

int
is_zero_number (Number n)
{
  return EMPTY (n[0]);
}

int
cmp_number (Number a, Number b)
{
  assert (is_normalized_number (a));
  assert (is_normalized_number (b));
  unsigned m = COUNT (a[0]), n = COUNT (b[0]);
  if (m < n)
    return -1;
  if (m > n)
    return 1;
  const unsigned *p = a[0].start, *q = b[0].start;
  while (p != a[0].top)
    {
      unsigned c = *p++, d = *q++;
      if (c < d)
	return -1;
      if (c > d)
	return 1;
    }
  return 0;
}

void
copy_number (Number dst, const Number src)
{
  CLEAR (dst[0]);
  RESERVE (dst[0], SIZE (src[0]));
  long n = COUNT (src[0]);
  dst[0].top = dst[0].start + n;
  memcpy (dst[0].start, src[0].start, n * sizeof *dst[0].start);
}

void
clear_number (Number n)
{
  RELEASE (n[0]);
}

void
print_number_as_sum_of_powers_of_two_to_file (Number n, FILE * file)
{
  const int size = COUNT (n[0]);
  int non_zero = 0;
  for (int i = size - 1; i >= 0; i--)
    {
      unsigned m = n[0].start[i];
      if (!m)
	continue;
      if (non_zero++)
	fputs (" + ", file);
      fprintf (file, "%u", m);
      if (i)
	fprintf (file, "*2^%d", 32 * i);
    }
  if (!non_zero)
    fputc ('0', file);
}

static void
print_number_to_stack (Number n, CharStack * s)
{
  int k = COUNT (n[0]);
  if (!k)
    {
      PUSH (*s, '0');
      return;
    }
  unsigned *q;
  ALLOC (q, k);
  memcpy (q, n[0].start, k * sizeof (unsigned));
  int i = k - 1;
  for (;;)
    {
      unsigned r = 0;
      while (i >= 0 && !q[i])
	i--;
      if (i < 0)
	break;
      for (int j = i; j >= 0; j--)
	{
	  unsigned old_t = q[j], old_r = r, t;
	  assert (old_r < 10);
	  uint64_t tl = old_t, rl;
	  tl += (uint64_t) old_r << 32;
	  rl = tl % 10;
	  tl = tl / 10;
	  assert (tl <= (uint64_t) UINT_MAX);
	  r = rl, t = tl;
	  assert (r < 10);
	  q[j] = t;
	}
      assert (r < 10);
      unsigned char c = r + '0';
      PUSH (*s, c);
    }
  DEALLOC (q, k);
}

static void
normalize_number (Number n)
{
  while (!EMPTY (n[0]) && !TOP (n[0]))
    (void) POP (n[0]);
  assert (is_normalized_number (n));
}

void
multiply_number_by_power_of_two (Number n, Exponent e)
{
  const uint64_t old_count = COUNT (n[0]);
  if (!old_count)
    return;
  const uint64_t word = e >> 5;
  const unsigned bit = e & 31;
  const uint64_t new_count = old_count + word + (bit != 0);
  while (new_count > SIZE (n[0]))
    ENLARGE (n[0]);
  n[0].top = n[0].start + new_count;
  unsigned src = old_count, dst = new_count;
  unsigned *start = n[0].start;
  if (bit)
    start[--dst] = start[src - 1] >> (32 - bit);
  while (src)
    {
      const unsigned data = start[--src];
      const unsigned prev = bit && src ? start[src - 1] : 0;
      start[--dst] = (data << bit) | (prev >> (32 - bit));
    }
  while (dst)
    start[--dst] = 0;
  normalize_number (n);
}

void
add_power_of_two_to_number (Number n, Exponent e)
{
  assert (n);
  uint64_t word = e >> 5;
  const unsigned bit = e & 31;
  unsigned inc = 1u << bit;
  for (;;)
    {
      if (word < COUNT (n[0]))
	{
	  unsigned before = n[0].start[word];
	  unsigned after = before + inc;
	  n[0].start[word] = after;
	  if (after)
	    break;
	  inc = 1, word++;
	}
      else
	PUSH (n[0], 0);
    }
}

void
sub_power_of_two_from_number (Number n, Exponent e)
{
  assert (n);
  uint64_t word = e >> 5;
  const unsigned bit = e & 31;
  unsigned dec = 1u << bit;
  unsigned *words = n[0].start;
  while (dec)
    {
      assert (word < COUNT (n[0]));
      unsigned before = words[word];
      unsigned after = before - dec;
      words[word] = after;
      if (after < before)
	break;
      dec = 1, word++;
    }
  normalize_number (n);
}

void
add_number (Number res, const Number other)
{
  const unsigned res_count = COUNT (res[0]);
  const unsigned other_count = COUNT (other[0]);
  uint64_t carry = 0;
  for (unsigned i = 0; i < other_count; i++)
    {
      carry += other[0].start[i];
      if (i < res_count)
	{
	  carry += res[0].start[i];
	  res[0].start[i] = (unsigned) carry;
	}
      else
	PUSH (res[0], (unsigned) carry);
      carry >>= 32;
    }
  if (carry)
    PUSH (res[0], (unsigned) carry);
}

void
sub_number (Number res, const Number other)
{
  const unsigned res_count = COUNT (res[0]);
  const unsigned other_count = COUNT (other[0]);
  uint64_t carry = 0;
  for (unsigned i = 0; i < other_count; i++)
    {
      carry += other[0].start[i];
      if (i < res_count)
	{
	  carry = res[0].start[i] - carry;
	  res[0].start[i] = (unsigned) carry;
	}
      else
	assert (!carry);
      carry >>= 32;
    }
  assert (!carry);
  normalize_number (res);
}

void
print_number_to_file (Number n, FILE * file)
{
  if (is_zero_number (n))
    {
      fputc ('0', file);
      return;
    }
  CharStack stack;
  INIT (stack);
  print_number_to_stack (n, &stack);
  while (!EMPTY (stack))
    fputc (POP (stack), stdout);
  RELEASE (stack);
}

/*------------------------------------------------------------------------*/
#endif // end of our own multiple precision number
/*------------------------------------------------------------------------*/

void
inc_number (Number n)
{
  add_power_of_two_to_number (n, 0);
}

void
println_number_to_file (Number n, FILE * file)
{
  print_number_to_file (n, file);
  fputc ('\n', file);
}

void
println_number (Number n)
{
  println_number_to_file (n, stdout);
}

#ifndef NLOG

void
log_number (Number n, const char *fmt, ...)
{
  va_list ap;
  fputs ("c LOG ", stdout);
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc (' ', stdout);
  print_number_to_file (n, stdout);
  fputc ('\n', stdout);
  fflush (stdout);
}

#endif
