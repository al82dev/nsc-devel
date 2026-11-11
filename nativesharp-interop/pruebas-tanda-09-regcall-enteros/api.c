#include "api.h"
unsigned long long __attribute__((regcall)) t09_regcall(unsigned int a, long long b, unsigned long long c) {
    if (a != 4000000000u || b != -5000000000LL || c != ~0ULL) return 0;
    return 0x8000000000000001ULL;
}
