#include "api.h"
__declspec(selectany) int _fltused = 0;
T09Small __attribute__((sysv_abi)) sysv_floats_record(float b, T09Small a, float c) {
    a.x += b + c; a.w += b + c; return a;
}
