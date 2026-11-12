#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09Wide {
float __attribute__((regcall)) sum(PairDouble value, int bias) {
    float result=(float)(value.x + value.y*2 + value.z*3 + bias);
    value.x=999.0;
    return result;
}
SumFn fetch() { return sum; }
}
