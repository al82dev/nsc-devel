#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
T09Hfa __attribute__((regcall)) t09_hfa_transform(T09Hfa value)
{
    value.x = value.x + (float)3;
    value.y = value.y - (float)5;
    return value;
}
T09HfaFn t09_hfa_get(void) { return t09_hfa_transform; }
