#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
Result __attribute__((regcall)) combine(Triple value, int bias) {
    Result result = {(int)value.pair.x + bias, (int)value.pair.y + (int)value.z, 3, 4, 5, 6};
    value.pair.x = 999.0f;
    return result;
}
CombineFn get_combine(void) { return combine; }

