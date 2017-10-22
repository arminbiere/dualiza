typedef struct Number Number;

struct Number {
  STACK (unsigned) stack;
};

Number * new_number ();
void add_power_of_two_to_number (Number * number, int);
void delete_number (Number *);

void print_number_to_file (Number *, FILE *);
void println_number_to_file (Number *, FILE *);
void println_number (Number *);
