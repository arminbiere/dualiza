#include <stdio.h>
#include <stdlib.h>

int main (int argc, char ** argv) {
  if (argc != 2) {
    fprintf (stderr, "*** permute: expected exactly one argument\n");
    exit (1);
  }
  int n = atoi (argv[1]);
  if (n < 2) {
    fprintf (stderr, "*** permute: invalid argument '%s'\n", argv[1]);
    exit (1);
  }
  int clauses = 0;

  for (int i = 1; i <= n; i++) {
    for (int j = 1; j <= n; j++) {
      for (int k = j + 1; k <= n; k++) {
	if (j == k) continue;
	if (clauses++) printf (" &\n");
	printf ("!(p%dh%d & p%dh%d)", j, i, k, i);
      }
    }
  }

  for (int i = 1; i <= n; i++) {
    if (clauses++) printf (" &\n");
    printf ("(");
    for (int j = 1; j <= n; j++) {
      if (j > 1) printf (" | ");
      printf ("p%dh%d", i, j);
    }
    printf (")");
  }

  printf ("\n");
  return 0;
}
