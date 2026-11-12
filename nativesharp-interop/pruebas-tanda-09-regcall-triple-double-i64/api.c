#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
long long __attribute__((regcall)) sum_pair(PairDouble value, int bias) {
    long long result = (long long)(value.x + value.y * 2 + value.z*3 + bias) + 4294967296LL;
    value.x=999.0f;
    return result;
}
SumFn get_sum(void) { return sum_pair; }
