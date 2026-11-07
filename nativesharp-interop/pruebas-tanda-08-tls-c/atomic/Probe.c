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
        atomic_tls = 0;
        if (atomic_start_worker() != 0) result = 5;
        else {
            for (int i = 0; i < 10000; ++i) atomic_tls += 1;
            if (atomic_wait_worker() != 0 || atomic_tls != 20000) result = 6;
        }
    }
    ExitProcess((unsigned int)result);
}
