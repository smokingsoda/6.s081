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

void h(void) {
  printf("嘿！！！！我在这！");
}

void main(void) {
  printf("%d %d\n", f(8)+1, 13);
  h();
  exit(0);
}
