#include "Api.h"
#include "Thread.h"
typedef void* Handle;
typedef unsigned long Dword;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
static Handle worker_handle;
static Dword __stdcall worker(void* unused)
{
    (void)unused;
    for (int i = 0; i < 10000; ++i) atomic_global += 1;
    return 0;
}
int start_atomic_worker(void)
{
    worker_handle = CreateThread(0, 0, worker, 0, 0, 0);
    return worker_handle ? 0 : 1;
}
int wait_atomic_worker(void)
{
    if (!worker_handle) return 1;
    Dword result = WaitForSingleObject(worker_handle, 0xffffffffUL);
    CloseHandle(worker_handle);
    worker_handle = 0;
    return result == 0 ? 0 : 2;
}
