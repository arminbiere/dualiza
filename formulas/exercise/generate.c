#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define M 10
#define N 100
#define K 7

int cnf[M + 1][K + 1];
int count[10];

int m, n, k;

int pick_interval (int l, int r) {
  return l + (rand () / (double) RAND_MAX * (r - l) + 0.5);
}

int pick_variable () { return pick_interval (1, m); }

int pick_bool () {
  return (rand () / (double) RAND_MAX) > 0.5;
}

int pick_sign () {
  return pick_bool () ? -1 : 1;
}

int pick_literal () {
  return pick_sign () * pick_variable ();
}

void pick_clause (int * c) {
  for (int i = 0; i < k; i++) {
    for (;;) {
      c[i] = pick_literal (i);
      int found = 0;
      for (int j = 0; !found && j < i; j++)
	found = (abs (c[j]) == abs (c[i]));
      if (!found) break;
    }
  }
}

void generate () {
  for (int i = 0; i < n; i++)
    pick_clause (cnf[i]);
}

void print (FILE * file) {
  fprintf (file, "p cnf %d %d\n", m, n);
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < k; j++)
      fprintf (file, "%d ", cnf[i][j]);
    fprintf (file, "0\n");
  }
}

int solutions () {
  FILE * file = fopen ("tmp.cnf", "w");
  print (file);
  fclose (file);
  FILE * pipe = popen ("dualiza tmp.cnf|tail -1", "r");
  int res = 0;
  (void) fscanf (pipe, "%d", &res);
  fclose (pipe);
  return res;
}

int count[10], all = 0;

int main () {
  srand (42);
  for (;;) {
    m = 5; // pick_interval (5, 6);
    n = m * (pick_interval (320, 400)/100.0);
    k = 4; //pick_interval (4);
    generate ();
    //print (stdout);
    int res = solutions ();
    //printf ("c solutions %d\n", res);
    if (res < 5) continue;
    if (res > 14) continue;
    res -= 5;
    assert (0 <= res), assert (res < 10);
    if (count[res] == 10) continue;
    char id[3];
    id[0] = '0' + count[res];
    id[1] = '0' + (res + 5) % 10;
    id[2] = 0;
    char name[10];
    sprintf (name, "%s.cnf", id);
    printf ("%s\n", name), fflush (stdout);
    FILE * file = fopen (name, "w");
    fprintf (file, "cnf %s\n", id);
    print (file);
    fclose (file);
    if (++count[res] < 10) continue;
    if (++all == 10) return 0;
  }
}
