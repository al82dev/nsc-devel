#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
Result __attribute__((regcall)) combine(int bias) {
    Result result = {10.0 + bias + 0.25, 50.5};
    return result;
}
CombineFn get_combine(void) { return combine; }

