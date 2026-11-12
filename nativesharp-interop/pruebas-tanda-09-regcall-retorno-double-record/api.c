#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
Result __attribute__((regcall)) combine(Triple value, int bias) {
    Result result = {value.pair.x + bias + 0.25, value.pair.y + value.z + 0.5};
    value.pair.x=999.0f;
    return result;
}
CombineFn get_combine(void) { return combine; }
