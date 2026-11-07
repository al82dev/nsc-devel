#include "Api.h"
typedef unsigned long Dword;
typedef void* Handle;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
__declspec(thread) int shared_tls = 3;
int tls_read(void) { return shared_tls; }
static Dword __stdcall worker(void* context)
{
    int* result = (int*)context;
    if (shared_tls != 3) { *result = 1; return 0; }
    shared_tls = 73;
    *result = shared_tls == 73 ? 0 : 2;
    return 0;
}
int tls_worker_check(void)
{
    int result = 99;
    Handle thread = CreateThread(0, 0, worker, &result, 0, 0);
    if (!thread) return 1;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 && result == 0 ? 0 : 2;
}
