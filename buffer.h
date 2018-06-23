typedef struct Buffer Buffer;
typedef struct Coo Coo;
typedef struct Coo Coo;

struct Coo { int code; long line, column, bytes; };

typedef STACK (Coo) CooStack;

struct Buffer { CooStack stack; long head; };

Buffer * new_buffer ();
int empty_buffer (Buffer *);
void enqueue_buffer (Buffer *, Coo);
Coo dequeue_buffer (Buffer *);
void delete_buffer (Buffer *);
