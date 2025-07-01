#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
  int original_page_cnt[2];
  int page_cnt[2];
  if(procpgdirinfo(original_page_cnt) == -1) {
    printf(1,"XV6_TEST_ERROR Error, procpgdirinfo returned -1\n");
  }
  // printf(1, "XV6_TEST_OUTPUT Start of program - base pages:%d huge pages:%d\n", original_page_cnt[0], original_page_cnt[1]);
  // printf(1, "XV6_TEST_OUTPUT Start of program\n");

  char* a = (char *)malloc(100);
  memset(a, 0, 100);
  if(procpgdirinfo(page_cnt) == -1) {
    printf(1,"XV6_TEST_ERROR Error, procpgdirinfo returned -1\n");
  }
  printf(1, "XV6_TEST_OUTPUT After first umalloc, 100 bytes - program base pages:%d huge pages:%d\n", page_cnt[0] - original_page_cnt[0], page_cnt[1] - original_page_cnt[1]);

  char* b = (char *)malloc(5000);
  memset(b, 0, 5000);
  if(procpgdirinfo(page_cnt) == -1) {
    printf(1,"XV6_TEST_ERROR Error, procpgdirinfo returned -1\n");
  }
  printf(1, "XV6_TEST_OUTPUT After second umalloc, 5000 bytes - program base pages:%d huge pages:%d\n", page_cnt[0] - original_page_cnt[0], page_cnt[1] - original_page_cnt[1]);

  char* c = (char *)malloc(4194304);
  memset(c, 0, 4194304);
  if(procpgdirinfo(page_cnt) == -1) {
    printf(1,"XV6_TEST_ERROR Error, procpgdirinfo returned -1\n");
  }
  printf(1, "XV6_TEST_OUTPUT After third umalloc, 4MB - program base pages:%d huge pages:%d\n", page_cnt[0] - original_page_cnt[0], page_cnt[1] - original_page_cnt[1]);

exit();
}
