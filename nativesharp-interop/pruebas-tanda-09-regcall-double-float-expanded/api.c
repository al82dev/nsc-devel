#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
int __attribute__((regcall)) sum_pair(PairDouble value, int bias, PairFloat extra) {
    int result=(int)(value.x + value.y*2 + value.z*3 + bias + extra.x*4 + extra.y*5);
    value.x=999.0; extra.x=999.0f;
    return result;
}
SumFn get_sum(void) { return sum_pair; }
