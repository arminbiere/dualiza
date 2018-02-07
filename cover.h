#define cover(C) \
do { \
  if (!(C)) break; \
  fflush (stdout); \
  fprintf (stderr, \
    "dualiza: %s:%d: %s: Coverage goal `%s' reached\n", \
    __FILE__, __LINE__, __FUNCTION__, #C); \
  fflush (stderr); \
  abort (); \
} while (0)
