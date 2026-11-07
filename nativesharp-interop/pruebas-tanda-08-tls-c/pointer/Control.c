#include "StaticApi.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (tls_pointer) ExitProcess(1);
    tls_pointer = pointer_target();
    if (*tls_pointer != 37) ExitProcess(2);
    *tls_pointer = 41;
    ExitProcess(pointer_check() || pointer_other_thread() || pointer_check() ? 3 : 0);
}
