#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09Wide {
bool __attribute__((regcall)) sum(PairDouble value, int bias) {
    bool result=(value.x == 10.25f && value.y == 20.5 && bias == 7);
    value.x=999.0;
    return result;
}
SumFn fetch() { return sum; }
}


