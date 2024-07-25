#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x) {
  return x+3;
}

int f(int x) {
  return g(x);
}

int *h(void) {
  int *a = malloc(3 * sizeof(int));
  a[0] = 1;
  return a;
}

void main(void) {
  printf("%d %d\n", f(8)+1, 13);
  exit(0);
}
