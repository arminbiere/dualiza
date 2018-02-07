#define cover(C) \
do { \
  if (!(C)) break; \
  fflush (stdout); \
  fprintf (stderr, \
    "*** 'dualiza' in '%s' line '%d: Coverage goal `%s' reached\n", \
    __FILE__, __LINE__, #C); \
  fflush (stderr); \
  abort (); \
} while (0)
