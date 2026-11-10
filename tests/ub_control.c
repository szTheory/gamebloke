/* Negative control for the sanitizer lane. It exists only to prove that the
 * ASan + UBSan lane aborts on undefined behavior, and is built only when
 * GBL_LANE is asan (never in the ci lane).
 *
 * The shift amount is opaque (volatile, derived from argc), so no compiler can
 * fold it into a constant and hide the undefined behavior. With no arguments
 * argc is 1 and the exponent is 32, which is too large for a 32-bit type.
 */
#include <stdint.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argv;
    volatile unsigned n = (unsigned)(argc + 31);
    uint32_t y = 1u << n;
    printf("%u\n", (unsigned)y);
    return 0;
}
