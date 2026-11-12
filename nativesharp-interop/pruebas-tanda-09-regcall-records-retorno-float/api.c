#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
float __attribute__((regcall)) combine(Triple a, int bias, Other b) {
    return (int)a.pair.x + 2*(int)a.pair.y + 3*(int)a.z + bias + 5*(int)b.pair.x + 7*(int)b.pair.y + 0.5;
}
CombineFn get_combine(void) { return combine; }
