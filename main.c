#include "headers.h"

int main (int argc, char ** argv) {
  STACK (int) s;
  INIT (s);
  for (int i = 0; i < 100; i++) PUSH (s, i);
  while (!EMPTY (s)) printf ("%d\n", POP (s));
  RELEASE (s);
  return 0;
}
