#include "api.h"
__declspec(selectany) int _fltused = 0;
int main(void) {
    T09SysvCallback callback = t09_get_sysv_callback();
    if (callback(1, 2, 3, 4, 5, 6) != 91) return 1;
    if (t09_get_float_callback()(1.5, 2.25f, 3, 4.0) != 10.75) return 2;
    if (t09_get_float_one_callback()(2.5f) != 3.5f) return 3;
    if (t09_get_wide_callback()(-5000000000LL, 7U) != -4999999993LL) return 4;
    if (t09_get_uint_callback()(4000000000U) != 4000000007U) return 7;
    if (t09_get_ulong_callback()(5000000000ULL, 7U) != 5000000007ULL) return 8;
    int x = 10;
    if (t09_get_pointer_callback()(&x, 3) != &x || x != 13) return 5;
    t09_get_void_callback()(&x);
    return x == 47 ? 0 : 6;
}
