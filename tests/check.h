/* Minimal in-tree test checks (D-06): no framework, header-only. A test
 * program includes this, calls CHECK / CHECK_STREQ, and returns CHECK_DONE()
 * from main. Each failure prints "<file>:<line>: check failed: <expr>" to
 * stderr and counts; CHECK_DONE() is 0 when nothing failed, 1 otherwise. */
#ifndef GBL_TESTS_CHECK_H
#define GBL_TESTS_CHECK_H

#include <stdio.h>
#include <string.h>

static int gbl_check_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
            gbl_check_failures++;                                            \
        }                                                                    \
    } while (0)

/* NULL on either side is a failure, never a crash. */
#define CHECK_STREQ(a, b)                                                    \
    do {                                                                     \
        const char *gbl_check_a = (a);                                       \
        const char *gbl_check_b = (b);                                       \
        if (gbl_check_a == NULL || gbl_check_b == NULL ||                    \
            strcmp(gbl_check_a, gbl_check_b) != 0) {                         \
            fprintf(stderr, "%s:%d: check failed: %s == %s\n", __FILE__,     \
                    __LINE__, #a, #b);                                       \
            gbl_check_failures++;                                            \
        }                                                                    \
    } while (0)

#define CHECK_DONE() (gbl_check_failures == 0 ? 0 : 1)

#endif /* GBL_TESTS_CHECK_H */
