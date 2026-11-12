#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
unsigned long long __attribute__((regcall)) combine(Triple a, int bias, Other b) {
    return 9223372036854775808ULL + (int)a.pair.x + 2*(int)a.pair.y + 3*(int)a.z + bias + 5*(int)b.pair.x + 7*(int)b.pair.y;
}
CombineFn get_combine(void) { return combine; }
