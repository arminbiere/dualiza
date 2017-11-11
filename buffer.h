typedef struct Buffer Buffer;
typedef struct Char Char;
typedef struct Coo Coo;

struct Coo { long line, column, bytes; };

struct Char { int code; Coo coo; };

struct Buffer { STACK (Char) stack; long head; };

Buffer * new_buffer ();
int empty_buffer (Buffer *);
void enqueue_buffer (Buffer *, Char);
Char dequeue_buffer (Buffer *);
void delete_buffer (Buffer *);
