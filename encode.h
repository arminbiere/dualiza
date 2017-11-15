Gate * decode_literal (Circuit *, int);
int encode_input (Circuit *, Gate *);

void print_dimacs_encoding_to_file (Circuit *, FILE *);
void print_dimacs_encoding (Circuit *);

void encode_circuit (Circuit *, CNF *, int negative);
void get_encoded_inputs (Circuit *, IntStack *);
