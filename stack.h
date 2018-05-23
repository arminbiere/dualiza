#define STACK(T) \
struct { T * start; T * top; T * end; }

#define INIT(S) \
do { \
  (S).start = (S).top = (S).end = 0; \
} while (0)

#define FULL(S) ((S).top == (S).end)

#define EMPTY(S) ((S).top == (S).start)

#define COUNT(S) ((size_t)((S).top - (S).start))

#define SIZE(S) ((size_t)((S).end - (S).start))

#define PEEK(S,I) \
  (assert ((I) < COUNT (S)), (S).start[I])

#define POKE(S,I,E) \
  (assert ((I) < COUNT (S)), (S).start[I] = (E))

#define CLEAR(S) \
do { \
  (S).top = (S).start; \
} while (0)

#define ENLARGE(S) \
do { \
  const long OLD_SIZE = SIZE (S); \
  const long NEW_SIZE = OLD_SIZE ? 2*OLD_SIZE : 1; \
  const long OLD_BYTES = OLD_SIZE * sizeof *(S).start; \
  const long NEW_BYTES = NEW_SIZE * sizeof *(S).start; \
  const long OLD_COUNT = COUNT (S); \
  REALLOC ((S).start, OLD_BYTES, NEW_BYTES); \
  (S).top = (S).start + OLD_COUNT; \
  (S).end = (S).start + NEW_SIZE; \
} while (0)

#define RESERVE(S,MIN_SIZE) \
do { \
  const long OLD_SIZE = SIZE (S); \
  if (OLD_SIZE >= (MIN_SIZE)) break; \
  long NEW_SIZE = OLD_SIZE; \
  do \
    NEW_SIZE = NEW_SIZE ? 2*NEW_SIZE : 1; \
  while (NEW_SIZE < (MIN_SIZE)); \
  const long OLD_COUNT = COUNT (S); \
  const long OLD_BYTES = OLD_SIZE * sizeof *(S).start; \
  const long NEW_BYTES = NEW_SIZE * sizeof *(S).start; \
  REALLOC ((S).start, OLD_BYTES, NEW_BYTES); \
  (S).top = (S).start + OLD_COUNT; \
  (S).end = (S).start + NEW_SIZE; \
} while (0)

#define PUSH(S,E) \
do { \
  if (FULL(S)) ENLARGE (S); \
  *(S).top++ = (E); \
} while (0)

#define POP(S) \
  (assert (!EMPTY (S)), *--(S).top)

#define TOP(S) \
  (assert (!EMPTY (S)), (S).top[-1])

#define RESIZE(S,N) \
do { \
  assert ((N) <= COUNT (S)); \
  (S).top = (S).start + (N); \
} while (0)

#define RELEASE(S) \
do { \
  DEALLOC ((S).start, SIZE (S)); \
  (S).start = (S).top = (S).end = 0; \
} while (0)

typedef STACK (int) IntStack;
typedef STACK (char) CharStack;
typedef STACK (unsigned) UnsignedStack;
