typedef struct Encoding Encoding;

struct Encoding {
  STACK (struct Gate *) inputs;
};

Encoding * new_encoding ();
Gate * decode_literal (Encoding *, int);
void delete_encoding (Encoding *);

void print_dimacs_encoding_to_file (Encoding *, FILE *);
void print_dimacs_encoding (Encoding *);

void get_encoded_inputs (Encoding *, IntStack *);
void only_encode_inputs (Circuit *, Encoding *);

void encode_circuit (Circuit *, CNF *, Encoding *, int negative);
