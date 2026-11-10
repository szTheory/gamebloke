/* Negative control for the sanitizer lane. It exists only to prove that the
 * ASan + UBSan lane aborts on a memory error, and is built only when GBL_LANE
 * is asan (never in the ci lane).
 *
 * With no arguments argc is 1, so the write lands one byte past the end of an
 * 8-byte heap block.
 */
#include <stdlib.h>

int main(int argc, char **argv)
{
    (void)argv;
    char *p = malloc(8);
    if (p == NULL) {
        return 1;
    }
    p[8 + argc - 1] = 1;
    free(p);
    return 0;
}
