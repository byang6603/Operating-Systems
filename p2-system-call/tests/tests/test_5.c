#include "types.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

int main(int argc, char* argv[]) {
    int fd = open("test_file.txt", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(2, "Error: Failed to open file\n");
        exit();
    }
    char * tw_1 = "it's a feature, not a bug! Right?\n";
    char * tw_2 = "Again... it's a feature?\n";
    write(fd, tw_1, strlen(tw_1));
    write(fd, tw_2, strlen(tw_2));
    printf(1, "XV6_TEST_OUTPUT Wrote to file!\n");
    close(fd);
    crash();
    printf(1, "XV6_TEST_OUTPUT Should not reach here.\n");

    exit();
}
