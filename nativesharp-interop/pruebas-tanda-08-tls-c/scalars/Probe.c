#include "Api.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void entry(void)
{
    int result = 0;
    if (tls_unsigned != 7 || tls_short != 2 || tls_double != 1.5 || tls_bool || tls_const != 19 || tls_volatile != 4) result = 1;
    else { tls_unsigned = 17; tls_short = 12; tls_double = 2.5; tls_bool = 1; tls_volatile = 14; if (scalar_check() || scalar_thread_check() || scalar_check()) result = 2; }
    ExitProcess((unsigned int)result);
}
