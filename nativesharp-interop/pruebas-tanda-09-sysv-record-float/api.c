#include "api.h"
__declspec(selectany) int _fltused = 0;
T09Small __attribute__((sysv_abi)) sysv_record_float(T09Small a, float b) {
    a.x += b; a.w += b; return a;
}
