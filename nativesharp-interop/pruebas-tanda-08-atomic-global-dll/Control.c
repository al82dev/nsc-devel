#include "Api.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (atomic_global != 3 || c_read_atomic_global() != 3) ExitProcess(1);
    if ((atomic_global += 2) != 5 || c_read_atomic_global() != 5) ExitProcess(2);
    if (enum_global != dll_low || c_read_enum_global() != 3) ExitProcess(3);
    enum_global += 2;
    if (enum_global != dll_high || c_read_enum_global() != 5) ExitProcess(4);
    ExitProcess(0);
}
