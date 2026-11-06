#include "TlsApi.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    int result = 0;
    if (tls_exported != 3) result = 1;
    else { tls_exported = 41; if (tls_read() != 41) result = 2; else if (tls_other_thread() != 0 || tls_exported != 41) result = 3; }
    ExitProcess((unsigned int)result);
}
