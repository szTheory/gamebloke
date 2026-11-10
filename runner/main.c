#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamebloke/gamebloke.h"

/* Documented exit codes: 0 success, 2 usage error. */
enum { GBL_RUN_USAGE_ERROR = 2 };

static void print_usage(FILE *out)
{
    fputs("usage: gamebloke-run [--version | --help]\n", out);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("gamebloke-run %s\n", gbl_version());
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        fputs("\n"
              "  --version  print the gamebloke-run version\n"
              "  --help     print this help\n"
              "\n"
              "exit codes: 0 success, 2 usage error\n",
              stdout);
        return EXIT_SUCCESS;
    }
    print_usage(stderr);
    return GBL_RUN_USAGE_ERROR;
}
