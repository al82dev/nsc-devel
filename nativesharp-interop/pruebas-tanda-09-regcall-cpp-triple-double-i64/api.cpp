#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09Wide {
long long __attribute__((regcall)) sum(PairDouble value, int bias) {
    long long result=(long long)(value.x + value.y*2 + value.z*3 + bias) + 4294967296LL;
    value.x=999.0;
    return result;
}
SumFn fetch() { return sum; }
}


