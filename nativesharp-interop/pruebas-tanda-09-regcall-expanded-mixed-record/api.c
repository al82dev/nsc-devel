#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
int __attribute__((regcall)) combine(Triple a, int bias, Other b) {
    int result = (int)a.pair.x + 2*(int)a.pair.y + 3*(int)a.z + bias + 5*(int)b.x + 7*(int)b.y;
    b.y = 999;
    return result;
}
CombineFn get_combine(void) { return combine; }
