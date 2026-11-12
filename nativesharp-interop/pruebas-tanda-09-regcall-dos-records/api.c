#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
Triple __attribute__((regcall)) combine(Triple a, Triple b, int bias) {
    a.pair.x += b.pair.x + (float)bias;
    a.pair.y += b.pair.y;
    a.z += b.z;
    return a;
}
CombineFn get_combine(void) { return combine; }
