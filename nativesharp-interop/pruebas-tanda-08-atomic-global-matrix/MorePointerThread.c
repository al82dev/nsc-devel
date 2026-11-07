#include "MorePointer.h"
#include "MorePointerThread.h"
typedef void* Handle;
typedef unsigned long Dword;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
static Handle handle;
static int worker_ok;
static float worker_float = 3.5f;
static long worker_long = 31;
static unsigned long worker_ulong = 41;
static Dword __stdcall worker(void* unused)
{
    (void)unused;
    if (more_float_ptr || more_long_ptr || more_ulong_ptr) return 1;
    more_float_ptr = &worker_float;
    more_long_ptr = &worker_long;
    more_ulong_ptr = &worker_ulong;
    worker_ok = *more_float_ptr == 3.5f && *more_long_ptr == 31 && *more_ulong_ptr == 41;
    return 0;
}
int more_pointer_start_worker(void)
{
    worker_ok = 0;
    handle = CreateThread(0, 0, worker, 0, 0, 0);
    return handle ? 0 : 1;
}
int more_pointer_wait_worker(void)
{
    if (!handle) return 1;
    Dword result = WaitForSingleObject(handle, 0xffffffffUL);
    CloseHandle(handle);
    handle = 0;
    return result == 0 && worker_ok ? 0 : 2;
}
