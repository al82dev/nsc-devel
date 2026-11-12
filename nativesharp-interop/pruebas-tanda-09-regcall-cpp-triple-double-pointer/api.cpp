#include "api.h"
#ifdef API_DLL_BUILD
extern "C" { int _fltused = 0; }
#endif
namespace T09Wide {
int* __attribute__((regcall)) sum(PairDouble value, int* out) {
    *out=(int)(value.x + value.y*2 + value.z*3); value.x=999.0f; return out;
}
SumFn fetch() { return sum; }
}
