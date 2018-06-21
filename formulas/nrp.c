#include <stdio.h>
#include <stdlib.h>
int n;
int main (int argc, char ** argv) {
  if (argc > 1) n = atoi (argv [1]);
  if (n < 3) n = 3;
  printf ("(x1");
  for (int i = 2; i <= n; i++)
    printf (" | x%d", i);
  printf (")");
  for (int i = 1; i <= n; i++) {
    printf (" |\n");
    printf ("(x%d", i + n);
    int first = 1;
    for (int j = 1; j <= n; j++) {
      if (i == j) continue;
      printf (" %c x%d", (first ? '=' : '^'), j);
      first = 0;
    }
    printf (")");
  }
  printf ("\n");
  return 0;
}
