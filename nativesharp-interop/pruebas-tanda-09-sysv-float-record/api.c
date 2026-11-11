#include "api.h"
__declspec(selectany) int _fltused = 0;
T09Small __attribute__((sysv_abi)) sysv_float_record(float b, T09Small a) {
    a.x += b; a.w += b; return a;
}
