#include "api.h"
__declspec(selectany) int _fltused = 0;
T09Small __attribute__((sysv_abi)) sysv_float_stack(T09Small a, float f1, double f2, float f3, double f4, float f5, double f6, float f7, double f8, float f9) {
    a.x += f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9; a.w += f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9; return a;
}
