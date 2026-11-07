#include "Api.h"
int main(void)
{
    if (shared_state != 7 || header_get() != 7) return 1;
    shared_state = 12;
    if (header_get() != 12) return 2;
    header_set(19);
    return shared_state == 19 ? 0 : 3;
}
