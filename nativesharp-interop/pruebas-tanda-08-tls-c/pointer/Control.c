#include "StaticApi.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (tls_pointer) ExitProcess(1);
    tls_pointer = pointer_target();
    if (*tls_pointer != 37) ExitProcess(2);
    pointer_prepare();
    tls_double_pointer = pointer_double_target();
    if (*tls_double_pointer != 1.5 || *tls_readonly_pointer != 37 || *tls_const_slot != 37) ExitProcess(4);
    *tls_double_pointer = 2.5; tls_volatile_slot = pointer_target(); *tls_volatile_pointer = 41;
    *tls_pointer = 41;
    ExitProcess(pointer_check() || pointer_other_thread() || pointer_check() ? 3 : 0);
}
