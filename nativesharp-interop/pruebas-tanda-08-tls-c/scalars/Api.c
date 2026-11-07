#include "Api.h"
typedef unsigned long Dword;
typedef void* Handle;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
__declspec(thread) unsigned int tls_unsigned = 7;
__declspec(thread) short tls_short = 2;
__declspec(thread) double tls_double = 1.5;
__declspec(thread) _Bool tls_bool = 0;
__declspec(thread) const int tls_const = 19;
__declspec(thread) volatile int tls_volatile = 4;
__declspec(thread) volatile unsigned int tls_volatile_unsigned = 5;
__declspec(thread) volatile long long tls_volatile_long = 6;
__declspec(thread) volatile double tls_volatile_double = 1.5;
__declspec(thread) volatile _Bool tls_volatile_bool = 0;
__declspec(thread) const volatile int tls_const_volatile = 23;
int scalar_check(void)
{
    return tls_unsigned == 17 && tls_short == 12 && tls_double == 2.5 &&
        tls_bool && tls_const == 19 && tls_volatile == 14 && tls_volatile_unsigned == 15 &&
        tls_volatile_long == 16 && tls_volatile_double == 2.5 && tls_volatile_bool && tls_const_volatile == 23 ? 0 : 1;
}
static Dword __stdcall worker(void* context)
{
    int* result = (int*)context;
    if (tls_unsigned != 7 || tls_short != 2 || tls_double != 1.5 ||
        tls_bool || tls_const != 19 || tls_volatile != 4 || tls_volatile_unsigned != 5 ||
        tls_volatile_long != 6 || tls_volatile_double != 1.5 || tls_volatile_bool || tls_const_volatile != 23) { *result = 1; return 0; }
    tls_unsigned = 27; tls_short = 22; tls_double = 3.5; tls_bool = 1; tls_volatile = 24;
    *result = tls_unsigned == 27 && tls_short == 22 && tls_double == 3.5 &&
        tls_bool && tls_volatile == 24 ? 0 : 2;
    return 0;
}
int scalar_thread_check(void)
{
    int result = 99;
    Handle thread = CreateThread(0, 0, worker, &result, 0, 0);
    if (!thread) return 1;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 && result == 0 ? 0 : 2;
}
