#include "api.h"
T09Small __attribute__((sysv_abi)) sysv_record_scalar(T09Small a, int b) {
    a.x += b; a.w += b; return a;
}
