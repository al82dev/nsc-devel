#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
Triple __attribute__((regcall)) combine(Triple a, int bias, Other b) {
    a.pair.x += b.x + (float)bias;
    a.pair.y += (float)b.y;
    a.z += b.x;
    b.y=999;
    return a;
}
CombineFn get_combine(void) { return combine; }
