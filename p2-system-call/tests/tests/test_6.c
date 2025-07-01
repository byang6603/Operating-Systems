#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
    char * tw_1 = "it's a feature, not a bug! This could be considered bad parenting.\n";
    write(1, tw_1, strlen(tw_1));

    int pid = fork();

    if (pid > 0)
        wait(); //Parent waits
    else
        crash(); //Child calls crash

    printf(1, "XV6_TEST_OUTPUT Should not reach this point.");
    exit();
}
