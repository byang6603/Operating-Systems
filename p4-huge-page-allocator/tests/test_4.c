#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {

  int original_page_cnt[2];
  int page_cnt[2];
  if(procpgdirinfo(original_page_cnt) == -1) {
    printf(1,"XV6_TEST_ERROR Error, procpgdirinfo returned -1\n");
  }
  // printf(1, "XV6_TEST_OUTPUT Start of program\n");
  // printf(1, "XV6_TEST_OUTPUT Start of program - base pages:%d huge pages:%d\n", original_page_cnt[0], original_page_cnt[1]);
  char *a = (char *)vmalloc(200, VMALLOC_SIZE_HUGE);
  memset(a, 0, 200);
  if(procpgdirinfo(page_cnt) == -1) {
    printf(1,"XV6_TEST_ERROR XV6_TEST_ERROR Error, procpgdirinfo returned -1\n");
  }
  printf(1, "XV6_TEST_OUTPUT After vmalloc huge, 200 bytes - program base pages:%d huge pages:%d\n", page_cnt[0] - original_page_cnt[0], page_cnt[1] - original_page_cnt[1]);
    exit();
}
