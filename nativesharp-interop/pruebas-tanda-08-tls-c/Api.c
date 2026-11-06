#include "Api.h"

typedef unsigned long WinDword;
typedef void* WinHandle;
typedef WinDword (__stdcall *WinThreadRoutine)(void*);
__declspec(dllimport) WinHandle __stdcall CreateThread(void*, unsigned long long, WinThreadRoutine, void*, WinDword, WinDword*);
__declspec(dllimport) WinDword __stdcall WaitForSingleObject(WinHandle, WinDword);
__declspec(dllimport) int __stdcall GetExitCodeThread(WinHandle, WinDword*);
__declspec(dllimport) int __stdcall CloseHandle(WinHandle);

static __declspec(thread) int tls_value = 3;

static WinDword __stdcall tls_worker(void* unused)
{
    (void)unused;
    if (tls_value != 3) return 1;
    tls_value = 73;
    return tls_value == 73 ? 0 : 2;
}

int verify_tls_isolation(void)
{
    if (tls_value != 3) return 10;
    tls_value = 41;
    WinHandle thread = CreateThread(0, 0, tls_worker, 0, 0, 0);
    if (!thread) return 11;
    WinDword wait = WaitForSingleObject(thread, 0xffffffffUL);
    WinDword result = 99;
    int got_result = GetExitCodeThread(thread, &result);
    CloseHandle(thread);
    if (wait != 0 || !got_result || result != 0) return 12;
    return tls_value == 41 ? 0 : 13;
}
