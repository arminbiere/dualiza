#include "stack.h"

typedef enum Operator Operator;
typedef struct Gate Gate;
typedef struct Circuit Circuit;

enum Operator { VAR, AND, ITE, XOR };

typedef STACK (Gate) Gates;

struct Gate {
  Operator op;
  int idx;
  Gates inputs;
  Gates outputs;
};

struct Circuit {
  int num_inputs;
  Gates gates;
};

Circuit * new_circuit (int num_inputs);
void delete_circuit (Circuit *);
