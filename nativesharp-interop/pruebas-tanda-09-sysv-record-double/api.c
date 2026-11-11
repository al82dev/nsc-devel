#include "api.h"
__declspec(selectany) int _fltused = 0;
T09Small __attribute__((sysv_abi)) sysv_record_double(T09Small a, double b) {
    a.x += b; a.w += b; return a;
}
