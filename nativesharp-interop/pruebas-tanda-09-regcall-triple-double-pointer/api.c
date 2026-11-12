#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
int* __attribute__((regcall)) sum_pair(PairDouble value, int* out) {
    *out=(int)(value.x + value.y*2 + value.z*3); value.x=999.0f; return out;
}
SumFn get_sum(void) { return sum_pair; }
