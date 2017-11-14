typedef STACK (struct Gate *) Encoding;

Encoding * new_encoding ();
Gate * decode_literal (Encoding *, int);
void delete_encoding (Encoding *);

void print_dimacs_encoding_to_file (Encoding *, FILE *);
void print_dimacs_encoding (Encoding *);

void encode_input (Encoding * e, Gate * g, int idx);

void get_encoded_inputs (Encoding *, IntStack *);

void encode_circuit (Circuit *, CNF *, Encoding *, int negative);
