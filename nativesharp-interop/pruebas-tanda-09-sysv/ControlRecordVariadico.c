#include "bad.h"

__declspec(selectany) int _fltused = 0;

static int check(T09Unsupported result)
{
    if (result.x != 7) return 1;
    if (result.y != 7) return 2;
    if (result.z != 1000) return 3;
    if (result.w != 3) return 4;
    if (result.extra != 350) return 5;
    return 0;
}

int main(void)
{
    T09Other value;
    value.x = 30064771077LL;
    value.y = 4000000000u;
    int code = check(sysv_record(2.5f, value, 3.25f));
    if (code != 0) return code;
    code = check(sysv_record(2.5f, value, 3.25f, 9));
    return code == 0 ? 0 : code + 10;
}
