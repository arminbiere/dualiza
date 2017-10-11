extern long allocated, max_allocated;

#define INC_ALLOCATED(B) \
do { \
  allocated += (B); \
  if (allocated > max_allocated) max_allocated = allocated; \
} while (0)

#define DEC_ALLOCATED(B) \
do { \
  assert (allocated >= (B)); \
  allocated -= (B); \
} while (0)

#define ALLOC(P,N) \
do { \
  if ((N)) { \
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
  if ((P)) { \
    assert ((N) > 0); \
    const long BYTES = (N) * sizeof *(P); \
    DEC_ALLOCATED (BYTES); \
    free (P); \
    (P) = 0; \
  } else assert (!(N)); \
} while (0)

#define REALLOC(P,O,N) \
do { \
  assert ((O) >= 0); \
  assert ((N) >= 0); \
  DEC_ALLOCATED (O); \
  (P) = realloc ((P), (N)); \
  if ((N) && !(P)) \
    die ("out-of-memory in '%s' reallocating %ld bytes", \
      __FUNCTION__, (long)(N)); \
  if ((O) < (N)) memset ((O) + (char*)(P), 0, (N) - (O)); \
  INC_ALLOCATED (N); \
} while (0)

#define NEW(P) ALLOC (P, 1)

#define DELETE(P) DEALLOC (P, 1)

