#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09Compact {
Result __attribute__((regcall)) combine(Triple value, int bias) {
    Result result{1099511627776LL + (int)value.pair.x + bias,
                  -1099511627776LL + (int)value.pair.y + (int)value.z, 3, 4, 5, 6};
    value.pair.x = 999.0f;
    return result;
}
CombineFn fetch() { return combine; }
}


