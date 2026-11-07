#include "Api.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void entry(void)
{
    int result = 0;
    if (atomic_tls != 3) result = 1;
    else { atomic_tls = 41; atomic_tls += 1; if (atomic_tls_read() != 42) result = 2; }
    ExitProcess((unsigned int)result);
}
