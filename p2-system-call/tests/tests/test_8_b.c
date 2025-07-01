#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
  printf(1, "XV6_TEST_OUTPUT In exec program! Calling crash.\n");
  crash();
  exit();
}
