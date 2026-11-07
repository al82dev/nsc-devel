#include "Api.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void entry(void)
{
    int result = 0;
    if (atomic_tls != 3) result = 1;
    else {
        atomic_tls = 41;
        atomic_tls += 1;
        if (atomic_tls_read() != 42) result = 2;
        else if (atomic_tls++ != 42 || --atomic_tls != 42) result = 3;
        else { atomic_tls -= 1; if (atomic_tls_read() != 41) result = 4; }
    }
    if (result == 0) {
        if ((atomic_tls &= 15) != 9) result = 9;
        else if ((atomic_tls |= 32) != 41) result = 10;
        else if ((atomic_tls ^= 3) != 42 || atomic_tls_read() != 42) result = 11;
        else if ((atomic_tls *= atomic_factor()) != 84 || atomic_tls_read() != 84 || atomic_factor_calls() != 1) result = 12;
    }
    if (result == 0) {
        if ((atomic_tls <<= 1) != 168 || (atomic_tls >>= 2) != 42) result = 15;
        else {
            atomic_tls = -84;
            if ((atomic_tls >>= 1) != -42 || atomic_tls_read() != -42) result = 16;
            else if ((atomic_tls /= 4) != -10 || atomic_tls_read() != -10) result = 17;
            else if ((atomic_tls %= 3) != -1 || atomic_tls_read() != -1) result = 18;
        }
    }
    if (result == 0) {
        atomic_tls = 1;
        if (atomic_start_mul_worker() != 0) result = 13;
        else {
            for (int i = 0; i < 10000; ++i) atomic_tls *= -1;
            if (atomic_wait_worker() != 0 || atomic_tls != -1) result = 14;
        }
    }
    if (result == 0) {
        atomic_tls = 0;
        if (atomic_start_worker() != 0) result = 5;
        else {
            for (int i = 0; i < 10000; ++i) atomic_tls += 1;
            if (atomic_wait_worker() != 0 || atomic_tls != 20000) result = 6;
        }
    }
    if (result == 0) {
        atomic_tls = 1;
        if (atomic_start_mul_worker() != 0) result = 19;
        else {
            for (int i = 0; i < 10000; ++i) atomic_tls /= 1;
            if (atomic_wait_worker() != 0 || atomic_tls != -1) result = 20;
        }
    }
    if (result == 0) {
        if (atomic_readonly != 7 || atomic_volatile != 9) result = 7;
        else {
            atomic_volatile = 10; atomic_volatile += 1;
            if (atomic_volatile_read() != 11) result = 8;
            else if ((atomic_volatile *= 2) != 22 || (atomic_volatile >>= 1) != 11) result = 21;
            else if ((atomic_volatile /= 2) != 5 || (atomic_volatile %= 3) != 2) result = 22;
            else if (atomic_volatile_read() != 2) result = 23;
        }
    }
    if (result == 0) {
        atomic_wide = 5000000000LL;
        if ((atomic_wide += 2) != 5000000002LL) result = 24;
        else if ((atomic_wide *= 2) != 10000000004LL) result = 25;
        else if ((atomic_wide >>= 1) != 5000000002LL) result = 26;
        else if ((atomic_wide /= 2) != 2500000001LL || atomic_wide != 2500000001LL) result = 27;
    }
    if (result == 0) {
        atomic_unsigned = 1U << 31;
        if ((atomic_unsigned /= 2U) != (1U << 30)) result = 34;
        atomic_unsigned = 1U << 31;
        if ((atomic_unsigned >>= 1) != (1U << 30)) result = 28;
        else if ((atomic_unsigned += 4U) != ((1U << 30) + 4U)) result = 29;
        else if ((atomic_unsigned /= 2U) != ((1U << 29) + 2U)) result = 30;
        atomic_unsigned_wide = 1ULL << 63;
        if ((atomic_unsigned_wide /= 2ULL) != (1ULL << 62)) result = 35;
        atomic_unsigned_wide = 1ULL << 63;
        if ((atomic_unsigned_wide >>= 1) != (1ULL << 62)) result = 31;
        else if ((atomic_unsigned_wide += 4ULL) != ((1ULL << 62) + 4ULL)) result = 32;
        else if ((atomic_unsigned_wide /= 2ULL) != ((1ULL << 61) + 2ULL)) result = 33;
    }
    if (result == 0) {
        if (atomic_float != 1.25f || atomic_double != 2.5 || atomic_float_const != 3.5f) result = 36;
        else {
            atomic_float = -7.5f;
            atomic_double = -12.25;
            if (atomic_float != -7.5f || atomic_double != -12.25) result = 37;
            else if ((atomic_float += 1.5f) != -6.0f) result = 38;
            else if ((atomic_float *= 2.0f) != -12.0f) result = 39;
            else if ((atomic_float /= 4.0f) != -3.0f) result = 40;
            else if ((atomic_float -= 1.0f) != -4.0f) result = 41;
            else if ((atomic_double += 0.25) != -12.0 || (atomic_double *= 2.0) != -24.0) result = 42;
            else if ((atomic_double /= 4.0) != -6.0 || (atomic_double -= 1.0) != -7.0) result = 43;
        }
    }
    if (result == 0) {
        if (atomic_bool || !atomic_bool_volatile || !atomic_bool_const) result = 44;
        else {
            atomic_bool = 1;
            atomic_bool_volatile = 0;
            if (!atomic_bool || atomic_bool_volatile) result = 45;
        }
    }
    if (result == 0) {
        if (atomic_pointer != 0) result = 46;
        else {
            atomic_pointer = atomic_pointer_expected();
            if (atomic_pointer != atomic_pointer_expected() || *atomic_pointer != 73) result = 47;
            else if (*(__c11_atomic_fetch_add(&atomic_pointer, 1, __ATOMIC_SEQ_CST) + 1) != 74) result = 51;
            else if (*(__c11_atomic_fetch_add(&atomic_pointer, 1, __ATOMIC_SEQ_CST) + 1) != 75) result = 52;
            else if (*(__c11_atomic_fetch_sub(&atomic_pointer, 2, __ATOMIC_SEQ_CST) - 2) != 73) result = 53;
            else if (*(__c11_atomic_fetch_add(&atomic_pointer, 1, __ATOMIC_SEQ_CST) + 1) != 74) result = 57;
            else if (*(__c11_atomic_fetch_add(&atomic_pointer, -1, __ATOMIC_SEQ_CST) - 1) != 73) result = 58;
            atomic_pointer = 0;
            if (atomic_pointer != 0) result = 48;
            else if (atomic_pointer_const != 0 || atomic_pointer_volatile != 0) result = 49;
            else {
                atomic_pointer_volatile = atomic_pointer_expected();
                if (atomic_pointer_volatile != atomic_pointer_expected()) result = 50;
                else if (__c11_atomic_fetch_add(&atomic_pointer_volatile, 1, __ATOMIC_SEQ_CST) != atomic_pointer_expected()) result = 54;
                else if (*(__c11_atomic_fetch_add(&atomic_pointer_volatile, 1, __ATOMIC_SEQ_CST) + 1) != 75) result = 55;
                else if (*__c11_atomic_fetch_sub(&atomic_pointer_volatile, 1, __ATOMIC_SEQ_CST) != 75 ||
                         *(__c11_atomic_fetch_sub(&atomic_pointer_volatile, 1, __ATOMIC_SEQ_CST) - 1) != 73) result = 56;
            }
        }
    }
    if (result == 0) {
        atomic_pointer_const_pointee = atomic_pointer_expected();
        if (*atomic_pointer_const_pointee != 73 || *(atomic_pointer_const_pointee + 1) != 74) result = 59;
    }
    if (result == 0) {
        atomic_pointer_volatile_pointee = atomic_pointer_expected();
        *atomic_pointer_volatile_pointee = 81;
        if (*atomic_pointer_volatile_pointee != 81) result = 60;
    }
    if (result == 0) {
        atomic_unsigned_pointer = atomic_unsigned_pointer_expected();
        if (*atomic_unsigned_pointer != 0xffffffffU) result = 61;
        else if (*(__c11_atomic_fetch_add(&atomic_unsigned_pointer, 1, __ATOMIC_SEQ_CST) + 1) != 74U) result = 62;
        else if (*(__c11_atomic_fetch_add(&atomic_unsigned_pointer, 1, __ATOMIC_SEQ_CST) + 1) != 75U) result = 63;
        else if (*(__c11_atomic_fetch_sub(&atomic_unsigned_pointer, 2, __ATOMIC_SEQ_CST) - 2) != 0xffffffffU) result = 64;
    }
    if (result == 0) {
        atomic_unsigned_const_pointee = atomic_unsigned_pointer_expected();
        if (*atomic_unsigned_const_pointee != 0xffffffffU || atomic_unsigned_const_pointee[1] != 74U) result = 65;
        atomic_unsigned_volatile_pointee = atomic_unsigned_pointer_expected();
        *atomic_unsigned_volatile_pointee = 81U;
        atomic_unsigned_volatile_pointee[1] = 82U;
        if (*atomic_unsigned_volatile_pointee != 81U || atomic_unsigned_volatile_pointee[1] != 82U) result = 66;
    }
    ExitProcess((unsigned int)result);
}
