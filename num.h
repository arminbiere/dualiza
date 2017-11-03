#ifdef GMP
#include <gmp.h>
typedef mpz_t Number;
#else
typedef UnsignedStack Number[1];
#endif

void init_number (Number);
void clear_number (Number);

void multiply_number_by_power_of_two (Number, int exponent);
void add_power_of_two_to_number (Number, int exponent);
void sub_power_of_two_from_number (Number, int exponent);

void print_number_to_file (Number, FILE *);
void println_number_to_file (Number, FILE *);
void println_number (Number);
