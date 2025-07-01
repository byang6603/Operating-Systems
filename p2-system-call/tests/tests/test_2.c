#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
    char * tw_2 = "it's a feature, not a bug!\n";
    write(1, tw_2, strlen(tw_2));
    crash();
    exit();
}
