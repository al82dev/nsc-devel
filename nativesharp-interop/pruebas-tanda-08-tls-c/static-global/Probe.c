#include "Api.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void entry(void)
{
    int result = 0;
    if (shared_tls != 3) result = 1;
    else { shared_tls = 41; if (tls_read() != 41) result = 2; else if (tls_worker_check() != 0 || shared_tls != 41) result = 3; }
    ExitProcess((unsigned int)result);
}
