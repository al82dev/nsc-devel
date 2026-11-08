#include "Api.h"
#include "Hilos.h"
typedef void* Handle;
typedef unsigned long Dword;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Handle __stdcall CreateEventW(void*, int, int, const unsigned short*);
__declspec(dllimport) int __stdcall SetEvent(Handle);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
static Handle worker_handle, ready_event, release_event;
static Dword __stdcall worker(void* unused)
{
    (void)unused;
    if (state != STATE_IDLE) return 1;
    state = STATE_IDLE;
    if (!SetEvent(ready_event)) return 2;
    if (WaitForSingleObject(release_event, 5000) != 0) return 3;
    return state == STATE_IDLE ? 0 : 4;
}
int state_start_worker(void)
{
    ready_event = CreateEventW(0, 0, 0, 0);
    release_event = CreateEventW(0, 0, 0, 0);
    if (!ready_event || !release_event) return 1;
    worker_handle = CreateThread(0, 0, worker, 0, 0, 0);
    if (!worker_handle) return 2;
    return WaitForSingleObject(ready_event, 5000) == 0 ? 0 : 3;
}
int state_finish_worker(void)
{
    Dword exit_code = 99;
    __declspec(dllimport) int __stdcall GetExitCodeThread(Handle, Dword*);
    if (!SetEvent(release_event) || WaitForSingleObject(worker_handle, 5000) != 0 ||
        !GetExitCodeThread(worker_handle, &exit_code)) return 1;
    CloseHandle(worker_handle);
    CloseHandle(ready_event);
    CloseHandle(release_event);
    return (int)exit_code;
}
