#include "api.h"
#ifdef API_DLL_BUILD
int _fltused = 0;
#endif
_Bool __attribute__((regcall)) sum_pair(PairDouble value, int bias) {
    _Bool result = (value.x == 10.25f && value.y == 20.5 && bias == 7);
    value.x=999.0f;
    return result;
}
SumFn get_sum(void) { return sum_pair; }
