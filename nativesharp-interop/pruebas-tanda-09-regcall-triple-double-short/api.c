#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
short __attribute__((regcall)) sum_pair(PairDouble value, int bias) {
    short result = (short)((int)(value.x + value.y * 2 + value.z*3 + bias) - 1292);
    value.x=999.0f;
    return result;
}
SumFn get_sum(void) { return sum_pair; }
