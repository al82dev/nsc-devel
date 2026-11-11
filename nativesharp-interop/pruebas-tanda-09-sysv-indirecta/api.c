#include "api.h"
static int __attribute__((sysv_abi)) weighted(int a, int b, int c, int d, int e, int f) {
    return a + 2*b + 3*c + 4*d + 5*e + 6*f;
}
T09SysvCallback t09_get_sysv_callback(void) { return weighted; }
