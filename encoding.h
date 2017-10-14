#include "stack.h"
typedef STACK (struct Gate *) Encoding;

Encoding * new_encoding ();
void encode_gate (Encoding *, Gate *, int);
Gate * decode_literal (Encoding *, int);
void delete_encoding (Encoding *);
