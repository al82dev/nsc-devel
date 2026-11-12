#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09Compact {
Result __attribute__((regcall)) combine(int bias) {
    Result result{10.0 + bias + 0.25, 50.5};
    return result;
}
CombineFn fetch() { return combine; }
}


