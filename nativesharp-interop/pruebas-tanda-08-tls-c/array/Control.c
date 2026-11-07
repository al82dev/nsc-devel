#include "StaticApi.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (tls_array[0] != 3 || tls_array[1] != 7 || tls_three[2] != 3 || tls_matrix[1][2] != 22 || tls_unsigned_array[1] != 5 || tls_double_array[0] != 1.5 || tls_bool_array[0] || tls_const_array[1] != 9 || tls_volatile_array[1] != 10) ExitProcess(1);
    tls_array[0] = 31; tls_array[1] = 71;
    tls_three[2] = 33; tls_matrix[1][2] = 42;
    tls_unsigned_array[1] = 55; tls_double_array[0] = 3.5; tls_bool_array[0] = 1; tls_volatile_array[1] = 100;
    ExitProcess(array_check() || array_other_thread() || array_check() ? 2 : 0);
}
