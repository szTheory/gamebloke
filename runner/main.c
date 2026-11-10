#include <stdio.h>
#include <string.h>

#include "gamebloke/gamebloke.h"

enum { GBL_RUN_USAGE_ERROR = 2 };

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("gamebloke-run %s\n", gbl_version());
        return 0;
    }
    fprintf(stderr, "usage: gamebloke-run [--version | --help]\n");
    return GBL_RUN_USAGE_ERROR;
}
