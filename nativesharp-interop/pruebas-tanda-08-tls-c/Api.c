#include "Api.h"

typedef unsigned long WinDword;
typedef void* WinHandle;
typedef WinDword (__stdcall *WinThreadRoutine)(void*);
__declspec(dllimport) WinHandle __stdcall CreateThread(void*, unsigned long long, WinThreadRoutine, void*, WinDword, WinDword*);
__declspec(dllimport) WinDword __stdcall WaitForSingleObject(WinHandle, WinDword);
__declspec(dllimport) int __stdcall CloseHandle(WinHandle);

static __declspec(thread) int tls_value = 3;

static WinDword __stdcall tls_worker(void* context)
{
    int* result = (int*)context;
    if (tls_value != 3) { *result = 1; return 0; }
    tls_value = 73;
    *result = tls_value == 73 ? 0 : 2;
    return 0;
}

int verify_tls_isolation(void)
{
    if (tls_value != 3) return 10;
    tls_value = 41;
    int result = 99;
    WinHandle thread = CreateThread(0, 0, tls_worker, &result, 0, 0);
    if (!thread) return 11;
    WinDword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    if (wait != 0 || result != 0) return 12;
    return tls_value == 41 ? 0 : 13;
}
