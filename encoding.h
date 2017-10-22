typedef struct Encoding Encoding;

struct Encoding {
  STACK (struct Gate *) stack;
};

Encoding * new_encoding ();
void encode_input (Encoding *, Gate *, int);
Gate * decode_literal (Encoding *, int);
void delete_encoding (Encoding *);

void print_dimacs_encoding_to_file (Encoding *, FILE *);
void print_dimacs_encoding (Encoding *);
