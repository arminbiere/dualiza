#include <stdio.h>
#include <stdlib.h>
int n;
int main (int argc, char ** argv) {
  if (argc > 1) n = atoi (argv [1]);
  if (n < 2) n = 2;
  if (n > 13) n = 13;
  printf ("(a");
  for (int i = 1; i < n; i++)
    printf (" | %c", 'a' + i);
  printf (")");
#if 0
  for (int i = 1; i < n; i++) {
    printf (" &\n");
    printf ("(%c = a", 'a' + i + n - 1);
    for (int j = 1; j <= i; j++)
      printf (" ^ %c", 'a' + j);
    printf (")");
  }
#endif
  printf ("\n");
  return 0;
}
