#ifndef _cover_h_INCLUDED
#define _cover_h_INCLUDED
#endif

#define COVER(COND) \
do { \
  if (!(COND)) break; \
  fflush (stdout); \
  fprintf (stderr, \
    "dualiza: %s:%d: %s: Coverage target `%s' reached.\n", \
    __FUNCTION__, __LINE__, __FILE__, # COND); \
  fflush (stderr);  \
  abort (); \
} while (0)
