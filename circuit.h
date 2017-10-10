typedef enum Operator Operator;
typedef struct Gate Gate;
typedef struct Circuit Circuit;

enum Operator { VAR, AND, ITE, XOR };

typedef struct Gates Gates;

struct Gates { Gate * start, * top, * end; };

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
