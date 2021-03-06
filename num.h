#ifndef NUM_H_INCLUDED
#define NUM_H_INCLUDED

#ifdef GMP
#include <gmp.h>
typedef mpz_t Number;
#else
typedef UnsignedStack Number[1];
#endif

typedef unsigned long Exponent;

void init_number (Number);
void init_number_from_unsigned (Number, unsigned);
void copy_number (Number, const Number);
void clear_number (Number);

int is_zero_number (Number);
int cmp_number (Number, Number);

void inc_number (Number);
void multiply_number_by_power_of_two (Number, Exponent);
void add_power_of_two_to_number (Number, Exponent);
void sub_power_of_two_from_number (Number, Exponent);

void add_number (Number, const Number);
void sub_number (Number, const Number);

void print_number_to_file (Number, FILE *);
void println_number_to_file (Number, FILE *);
void println_number (Number);

#endif
