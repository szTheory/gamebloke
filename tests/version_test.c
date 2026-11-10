#include "check.h"
#include "gamebloke/gamebloke.h"

int main(void)
{
    CHECK(gbl_version() != NULL);
    CHECK_STREQ(gbl_version(), "0.0.0");
    return CHECK_DONE();
}
