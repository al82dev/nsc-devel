#include "api.h"
int sysv_value;
int __attribute__((sysv_abi)) sysv_mix(int a, int b, int c, int d, int e, int f) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6;
}
void __attribute__((sysv_abi)) sysv_set(int value) { sysv_value = value; }
