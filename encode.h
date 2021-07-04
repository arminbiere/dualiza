int encode_input (Circuit *, Gate *);
Gate *decode_literal (Circuit *, int);

void print_dimacs_encoding_to_file (Circuit *, FILE *);
void print_dimacs_encoding (Circuit *);

void encode_circuit (Circuit *, CNF *);
void encode_circuits (Circuit *, Circuit *, CNF *, CNF *);
void get_encoded_inputs (Circuit *, IntStack *);
