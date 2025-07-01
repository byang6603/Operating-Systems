#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
    char * tw_1 = "it's a feature, not a bug!\n";
    write(1, tw_1, strlen(tw_1));
    printf(1, "XV6_TEST_OUTPUT Wrote the big bad string!\n");
    exit();
}
