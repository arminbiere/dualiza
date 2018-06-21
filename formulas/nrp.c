#include <stdio.h>
#include <stdlib.h>
int n;
int main (int argc, char ** argv) {
  if (argc > 1) n = atoi (argv [1]);
  if (n < 2) n = 2;
  printf ("(x1");
  for (int i = 2; i <= n; i++)
    printf (" | x%d", i);
  printf (")");
#if 1
  for (int i = 1; i < n; i++) {
    printf (" |\n");
    printf ("(x%d = x1", i + n);
    for (int j = 2; j <= i + 1; j++)
      printf (" ^ x%d", j);
    printf (")");
  }
#endif
  printf ("\n");
  return 0;
}
