#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
  int ret = crash();
  printf(1, "XV6_TEST_OUTPUT Hello world!\n");
  printf(1, "XV6_TEST_OUTPUT Crash returned %d.\n", ret);
  ret = crash();
  printf(1, "XV6_TEST_OUTPUT Crash returned %d.\n", ret);
  exit();
}
