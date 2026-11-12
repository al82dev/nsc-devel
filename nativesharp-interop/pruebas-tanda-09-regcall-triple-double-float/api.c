#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
float __attribute__((regcall)) sum_pair(PairDouble value, int bias) {
    float result=(float)(value.x + value.y*2 + value.z*3 + bias);
    value.x=999.0;
    return result;
}
SumFn get_sum(void) { return sum_pair; }
