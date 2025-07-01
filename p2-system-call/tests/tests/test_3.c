#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
    char * tw_1 = "Luckily, it's a feature, not a bug!\n";
    write(1, tw_1, strlen(tw_1));
    crash();
    printf(1, "XV6_TEST_OUTPUT So we see this!\n");

    char * tw_2 = "it's a feature, not a bug! Pretty Cool!\n";
    write(1, tw_2, strlen(tw_2));
    crash();
    printf(1, "XV6_TEST_OUTPUT But not this!\n");
    exit();
}
