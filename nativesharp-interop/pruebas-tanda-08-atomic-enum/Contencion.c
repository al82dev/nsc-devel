#include "Api.h"
#include "Contencion.h"
typedef void* Handle;
typedef unsigned long Dword;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Handle __stdcall CreateEventW(void*, int, int, const unsigned short*);
__declspec(dllimport) int __stdcall SetEvent(Handle);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall GetExitCodeThread(Handle, Dword*);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
static Handle thread_handle, ready_event, start_event;
static Dword __stdcall worker(void* unused)
{
    (void)unused;
    if (!SetEvent(ready_event) || WaitForSingleObject(start_event, 10000) != 0) return 1;
    for (int i = 0; i < 50000; ++i) state++;
    return 0;
}
int contention_start(void)
{
    ready_event = CreateEventW(0, 0, 0, 0);
    start_event = CreateEventW(0, 0, 0, 0);
    if (!ready_event || !start_event) return 1;
    thread_handle = CreateThread(0, 0, worker, 0, 0, 0);
    if (!thread_handle) return 2;
    return WaitForSingleObject(ready_event, 10000) == 0 ? 0 : 3;
}
int contention_release(void) { return SetEvent(start_event) ? 0 : 1; }
int contention_join(void)
{
    Dword code = 1;
    if (WaitForSingleObject(thread_handle, 10000) != 0 || !GetExitCodeThread(thread_handle, &code)) return 1;
    CloseHandle(thread_handle);
    CloseHandle(ready_event);
    CloseHandle(start_event);
    return (int)code;
}
