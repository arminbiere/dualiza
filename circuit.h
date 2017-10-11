#include "stack.h"

typedef enum Operator Operator;
typedef struct Gate Gate;
typedef struct Circuit Circuit;

enum Operator { INPUT, AND, ITE, XOR };

typedef STACK (Gate*) Gates;

struct Gate {
  Operator op;
  int idx, pos, neg;
  Gates inputs, outputs;
};

struct Circuit {
  int num_inputs;
  Gates gates;
};

#define FALSE ((Gate*)0l)
#define TRUE ((Gate*)1l)

#define NOT(G) ((Gate*)(1l^(long)(G)))
#define STRIP(G) ((Gate*)(~1l&(long)(G)))

Circuit * new_circuit (int num_inputs);
void delete_circuit (Circuit *);
