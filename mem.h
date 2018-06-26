#define INC_ALLOCATED(B) \
do { \
  stats.bytes.current += (B); \
  if (stats.bytes.current > stats.bytes.max) \
    stats.bytes.max = stats.bytes.current; \
} while (0)

#define DEC_ALLOCATED(B) \
do { \
  assert (stats.bytes.current >= (B)); \
  stats.bytes.current -= (B); \
} while (0)

#define ALLOC(P,N) \
do { \
  if ((N) != 0) { \
    assert ((N) > 0); \
    const long BYTES = (N) * sizeof *(P); \
    (P) = malloc (BYTES); \
    INC_ALLOCATED (BYTES); \
    if (!(P)) \
      die ("out-of-memory in '%s' allocating %ld bytes", \
	__FUNCTION__, BYTES); \
    memset ((P), 0, BYTES); \
  } else (P) = 0; \
} while (0)

#define DEALLOC(P,N) \
do { \
  const long BYTES = (N) * sizeof *(P); \
  DEC_ALLOCATED (BYTES); \
  free (P); \
} while (0)

#define REALLOC(P,O,N) \
do { \
  assert ((O) >= 0); \
  assert ((N) >= 0); \
  DEC_ALLOCATED (O); \
  (P) = realloc ((P), (N)); \
  if ((N) != 0 && !(P)) \
    die ("out-of-memory in '%s' reallocating %ld bytes", \
      __FUNCTION__, (long)(N)); \
  if ((O) < (N)) memset ((O) + (char*)(P), 0, (N) - (O)); \
  INC_ALLOCATED (N); \
} while (0)

#define NEW(P) ALLOC (P, 1)

#define DELETE(P) DEALLOC (P, 1)

#define STRDUP(P,S) \
do { \
  assert (S); \
  size_t LEN = strlen (S); \
  ALLOC ((P), LEN + 1); \
  strcpy ((P), (S)); \
} while (0)

#define STRDEL(S) \
do { \
  size_t LEN = strlen (S); \
  DEALLOC ((S), LEN + 1); \
} while (0)
