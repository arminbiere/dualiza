#include "stack.h"

typedef enum Operator Operator;
typedef struct Gate Gate;
typedef struct Circuit Circuit;

// using C operator precedence priorities as encoding

enum Operator { 
  FALSE = 0,
  INPUT = 1,
  AND = 8,
  XOR = 9,
  OR = 10,
  ITE = 13,
  XNOR = 14
};

typedef STACK (Gate*) Gates;

struct Gate {
  Operator op;
  int idx, pos, neg, input, mark;
  Gates inputs, outputs;
  Circuit * circuit;
};

struct Circuit {
  Gates inputs, gates;
  Gate * zero, * output;
};

#define NOT(G) ((Gate*)(1l^(long)(G)))
#define SIGN(G) ((int)(1l&(long)(G)))
#define STRIP(G) ((Gate*)(~1l&(long)(G)))

Circuit * new_circuit ();
void delete_circuit (Circuit *);

Gate * new_false_gate (Circuit *);
Gate * new_input_gate (Circuit *, int input);
Gate * new_and_gate (Circuit *);
Gate * new_xor_gate (Circuit *);
Gate * new_or_gate (Circuit *);
Gate * new_ite_gate (Circuit *);
Gate * new_xnor_gate (Circuit *);

void connect_gates (Gate * input, Gate * output);
void connect_output (Circuit *, Gate * output);

void print_circuit_to_file (Circuit *, FILE *);
void println_circuit_to_file (Circuit *, FILE *);
void println_circuit (Circuit *);
void println_gate (Gate *);

void check_circuit_connected (Circuit *);
