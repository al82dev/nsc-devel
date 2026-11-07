#include "Widths.h"
#include "WidthsThread.h"
typedef void* Handle;
typedef unsigned long Dword;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
static Handle handle;
static int worker_ok;
static signed char i8 = -21;
static unsigned char u8 = 22;
static unsigned short u16 = 23;
static unsigned long long u64 = 24;
static Dword __stdcall worker(void* unused)
{
    (void)unused;
    if (width_i8 || width_u8 || width_u16 || width_u64) return 1;
    width_i8 = &i8;
    width_u8 = &u8;
    width_u16 = &u16;
    width_u64 = &u64;
    worker_ok = *width_i8 == -21 && *width_u8 == 22 && *width_u16 == 23 && *width_u64 == 24;
    return 0;
}
int widths_start_worker(void)
{
    worker_ok = 0;
    handle = CreateThread(0, 0, worker, 0, 0, 0);
    return handle ? 0 : 1;
}
int widths_wait_worker(void)
{
    if (!handle) return 1;
    Dword result = WaitForSingleObject(handle, 0xffffffffUL);
    CloseHandle(handle);
    handle = 0;
    return result == 0 && worker_ok ? 0 : 2;
}
