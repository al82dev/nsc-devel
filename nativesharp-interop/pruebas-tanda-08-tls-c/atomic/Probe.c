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
        if (atomic_readonly != 7 || atomic_volatile != 9) result = 7;
        else { atomic_volatile = 10; atomic_volatile += 1; if (atomic_volatile_read() != 11) result = 8; }
    }
    ExitProcess((unsigned int)result);
}
