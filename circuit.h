typedef enum Operator Operator;
typedef struct Gate Gate;
typedef struct Circuit Circuit;

// using C operator precedence priorities as encoding

enum Operator { 
  FALSE_OPERATOR = 0,
  INPUT_OPERATOR = 1,
  AND_OPERATOR   = 8,
  XOR_OPERATOR   = 9,
  OR_OPERATOR    = 10,
  ITE_OPERATOR   = 13,
  XNOR_OPERATOR  = 14,
};

typedef STACK (Gate*) Gates;

struct Gate {
  Operator op;
  int idx, input, code;
  char pos, neg, mark, root;
  Gates inputs, outputs;
  Circuit * circuit;
  struct Symbol * symbol;
};

int get_gate_size (Gate *);
Gate * get_gate_input (Gate *, int);

struct Circuit {
  Gates inputs, gates;
  Gate * zero, * output;
};

Circuit * new_circuit ();
void delete_circuit (Circuit *);

Gate * new_false_gate (Circuit *);
Gate * new_input_gate (Circuit *);
Gate * new_and_gate (Circuit *);
Gate * new_xor_gate (Circuit *);
Gate * new_or_gate (Circuit *);
Gate * new_ite_gate (Circuit *);
Gate * new_xnor_gate (Circuit *);

const char * gate_name (Gate *);

void connect_gates (Gate * input, Gate * output);
void connect_output (Circuit *, Gate * output);
void check_circuit_connected (Circuit *);

void sort_circuit (Circuit *);
