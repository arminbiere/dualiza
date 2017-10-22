typedef struct Buffer Buffer;

struct Buffer {
  STACK (int) stack;
  long head;
};

Buffer * new_buffer ();
int empty_buffer (Buffer *);
void enqueue_buffer (Buffer *, int);
int dequeue_buffer (Buffer *);
void delete_buffer (Buffer *);
