#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {

    int pid = fork();

    if (pid > 0)
    {
        wait(); //Parent waits
        printf(1, "XV6_TEST_OUTPUT Parent calling crash!\n");
        crash();
    }
    else
    {
        char * tw_1 = "it's a feature, not a bug! Kids these days...\n";
        write(1, tw_1, strlen(tw_1));
        exit();
    }

    printf(1, "XV6_TEST_OUTPUT Parent exiting!!\n");
    exit();
}
