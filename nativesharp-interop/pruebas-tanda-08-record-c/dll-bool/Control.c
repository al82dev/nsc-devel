#include "Api.h"
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    if (flag || c_read_flag()) ExitProcess(1);
    flag = 1;
    if (!flag || c_read_flag() != 1) ExitProcess(2);
    c_write_flag(0);
    if (flag || c_read_flag()) ExitProcess(3);
    ExitProcess(0);
}
