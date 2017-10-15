#include "headers.h"

Buffer * new_buffer () {
  Buffer * res;
  NEW (res);
  return res;
}

void delete_buffer (Buffer * b) {
  RELEASE (b->stack);
  DELETE (b);
}

int empty_buffer (Buffer * b) {
  return COUNT (b->stack) == b->head;
}

static void compact_buffer (Buffer * b) {
  long count = COUNT (b->stack);
  long head = b->head;
  assert (head <= count);
  long actual = count - head;
  long bytes = actual * sizeof (int);
  int * start = b->stack.start;
  const int * src = start + head;
  memmove (start, src, bytes);
  RESIZE (b->stack, actual);
  b->head = 0;
  LOG ("compact buffer of size %ld count %ld head %ld",
    (long) SIZE (b->stack), count, head);
}

void enqueue_buffer (Buffer * b, int e) {
  long count = COUNT (b->stack);
  if (count > 8 && b->head >= count/2) compact_buffer (b);
  PUSH (b->stack, e);
}

int dequeue_buffer (Buffer * b) {
  assert (!empty_buffer (b));
  return b->stack.start[b->head++];
}
