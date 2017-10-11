extern long allocated;

#define ALLOC(P,N) \
do { \
  if ((N)) { \
    assert ((N) > 0); \
    const long BYTES = (N) * sizeof *(P); \
    (P) = malloc (BYTES); \
    allocated += BYTES; \
    if (!(P)) \
      die ("out-of-memory in '%s' allocating %ld bytes", \
	__FUNCTION__, BYTES); \
    memset ((P), 0, BYTES); \
  } else (P) = 0; \
} while (0)

#define DEALLOC(P,N) \
do { \
  if ((P)) { \
    assert ((N) > 0); \
    const long BYTES = (N) * sizeof *(P); \
    assert (allocated >= BYTES); \
    allocated -= BYTES; \
    free (P); \
    (P) = 0; \
  } else assert (!(N)); \
} while (0)

#define REALLOC(P,O,N) \
do { \
  assert ((O) >= 0); \
  assert ((N) >= 0); \
  assert (allocated >= (O)); \
  allocated -= (O); \
  (P) = realloc ((P), (N)); \
  if ((N) && !(P)) \
    die ("out-of-memory in '%s' reallocating %ld bytes", \
      __FUNCTION__, (long)(N)); \
  if ((O) < (N)) memset ((O) + (char*)(P), 0, (N) - (O)); \
  allocated += (N); \
} while (0)

#define NEW(P) ALLOC (P, 1)

#define DELETE(P) DEALLOC (P, 1)

