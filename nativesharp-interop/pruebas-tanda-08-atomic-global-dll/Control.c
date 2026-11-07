#include "Api.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (atomic_global != 3 || c_read_atomic_global() != 3) ExitProcess(1);
    if ((atomic_global += 2) != 5 || c_read_atomic_global() != 5) ExitProcess(2);
    ExitProcess(0);
}
