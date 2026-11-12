#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
int __attribute__((regcall)) sum_pair(PairDouble value, int bias) {
    int result=(int)(value.x + value.y*2 + bias);
    value.x=999.0;
    return result;
}
SumFn get_sum(void) { return sum_pair; }


