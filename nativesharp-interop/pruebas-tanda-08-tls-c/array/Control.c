#include "StaticApi.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (tls_array[0] != 3 || tls_array[1] != 7) ExitProcess(1);
    tls_array[0] = 31; tls_array[1] = 71;
    ExitProcess(array_check() || array_other_thread() || array_check() ? 2 : 0);
}
