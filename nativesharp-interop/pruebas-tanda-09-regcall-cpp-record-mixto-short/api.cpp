#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09Wide {
short __attribute__((regcall)) sum(PairDouble value, int bias) {
    short result=(short)((int)(value.x + value.y*2 + bias) - 1292);
    value.x=999.0;
    return result;
}
SumFn fetch() { return sum; }
}


