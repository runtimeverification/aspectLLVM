#include <stdio.h>

int inc(int i) {
  return i + 1;
}

void print(const char* str) {
  printf("%s", str);
}

int main() {
  print("Hello World!\n");
  int i = 21;
  i = inc(i);
  i = inc(i);
  i = inc(i);
  i = inc(i);
  i = inc(i);
  return 0;
}
