#ifdef GMP
#include <gmp.h>
typedef mpz_t Number;
#else
typedef UnsignedStack Number[1];
#endif

typedef unsigned long Exponent;

void init_number (Number);
void copy_number (Number dst, const Number src);
void clear_number (Number);

void multiply_number_by_power_of_two (Number, Exponent);
void add_power_of_two_to_number (Number, Exponent);
void sub_power_of_two_from_number (Number, Exponent);

void print_number_to_file (Number, FILE *);
void println_number_to_file (Number, FILE *);
void println_number (Number);
