#include "api.h"
T09Small __attribute__((sysv_abi)) sysv_scalar_record(int b, T09Small a) {
    a.x += b; a.w += b; return a;
}
