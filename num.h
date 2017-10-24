typedef UnsignedStack Number[1];

void init_number (Number);
void reset_number (Number);

void add_power_of_two_to_number (Number, int exponent);
void sub_power_of_two_from_number (Number, int exponent);

void print_number_to_file (Number, FILE *);
void println_number_to_file (Number, FILE *);
void println_number (Number);
