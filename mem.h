long allocated;

#define ALLOC(P,N) \
do { \
  assert (P); \
  if ((N)) { \
    assert ((N) > 0); \
    long BYTES = (N) * sizeof *(P); \
    (P) = malloc (BYTES); \
    allocated += BYTES; \
    if (!(P)) \
      die ("out-of-memory in '%s' allocating %ld bytes", \
	__FUNCTION__, BYTES); \
    memset ((P), 0, BYTES); \
  } else (P) = 0;
} while (0)

#define DEALLOC(P,N) \
do { \
  if ((P)) { \
    assert ((N) > 0); \
    long BYTES = (N) * sizeof *(P); \
    assert (allocated >= BYTES); \
    allocated_bytes -= BYTES; \
    if (P) free (P); \
    (P) = 0; \
  } else assert (!(N)); \
} while (0)

#define NEW(P) ALLOC (P, 1)
#define DELETE(P) DEALLOC (P, 1)

#define CLEAR(P) \
do { \
  assert (P); \
  memset ((P), 0, sizeof *(P)); \
} while (0)

#define REALLOC(P,O,N) \
do { \
  assert ((O) >= 0); \
  assert ((N) >= 0); \
  (P) = realloc ((P), (N)); \
  if ((N) && !(P)) \
    die ("out-of-memory in '%s' reallocating %ld bytes", \
      __FUNCTION__, (long)(N)); \
  if ((O) < (N)) memset ((P), 0, (N) - (O)); \
} while (0)
