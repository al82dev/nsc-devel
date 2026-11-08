#include "Api.h"
#include "Thread.h"
typedef void* Handle;
typedef unsigned long Dword;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
__declspec(dllimport) void __stdcall Sleep(Dword);
_Atomic(int) atomic_turn;
static _Atomic(int) worker_status;
static Handle worker_handle;
void yield_atomic_worker(void) { Sleep(0); }
static int wait_turn(int wanted)
{
    for (int spin = 0; spin < 1000000; ++spin) {
        if (atomic_turn == wanted) return 0;
        Sleep(0);
    }
    return 1;
}
static Dword __stdcall worker(void* unused)
{
    (void)unused;
    for (int round = 0; round < 100; ++round) {
        if (wait_turn(1)) { worker_status = 10; return 10; }
        atomic_turn = 2;
        if (wait_turn(3)) { worker_status = 11; return 11; }
        for (int i = 0; i < 100; ++i) atomic_global += 1;
        atomic_turn = 4;
        if (wait_turn(5)) { worker_status = 12; return 12; }
        atomic_turn = 0;
    }
    return 0;
}
int start_atomic_worker(void)
{
    worker_status = 0;
    worker_handle = CreateThread(0, 0, worker, 0, 0, 0);
    return worker_handle ? 0 : 1;
}
int wait_atomic_worker(void)
{
    if (!worker_handle) return 1;
    Dword result = WaitForSingleObject(worker_handle, 30000UL);
    int worker_result = worker_status;
    CloseHandle(worker_handle);
    worker_handle = 0;
    return result == 0 && worker_result == 0 ? 0 : 2;
}
