#include "Api.h"
_Thread_local _Atomic(int) atomic_tls = 3;
_Thread_local const _Atomic(int) atomic_readonly = 7;
_Thread_local volatile _Atomic(int) atomic_volatile = 9;
_Thread_local _Atomic(long long) atomic_wide = 10;
_Thread_local _Atomic(unsigned long long) atomic_unsigned_wide = 11;
_Thread_local _Atomic(unsigned int) atomic_unsigned = 12;
_Thread_local _Atomic(short) atomic_unsupported_short = 13;
_Thread_local _Atomic(signed char) atomic_narrow_char = 0;
_Thread_local _Atomic(unsigned short) atomic_narrow_ushort = 0;
_Thread_local volatile _Atomic(short) atomic_narrow_volatile = 0;
_Thread_local _Atomic(float) atomic_float = 1.25f;
_Thread_local volatile _Atomic(double) atomic_double = 2.5;
_Thread_local const _Atomic(float) atomic_float_const = 3.5f;
_Thread_local _Atomic(_Bool) atomic_bool = 0;
_Thread_local volatile _Atomic(_Bool) atomic_bool_volatile = 1;
_Thread_local const _Atomic(_Bool) atomic_bool_const = 1;
_Thread_local _Atomic(int*) atomic_pointer = 0;
_Thread_local volatile _Atomic(int*) atomic_pointer_volatile = 0;
_Thread_local const _Atomic(int*) atomic_pointer_const = 0;
_Thread_local _Atomic(const int*) atomic_pointer_const_pointee = 0;
_Thread_local _Atomic(volatile int*) atomic_pointer_volatile_pointee = 0;
_Thread_local _Atomic(unsigned int*) atomic_unsigned_pointer = 0;
_Thread_local _Atomic(struct AtomicUnsupported) atomic_unsupported_record;
static _Thread_local int atomic_pointer_target[3] = {73, 74, 75};
int* atomic_pointer_expected(void) { return atomic_pointer_target; }
static _Thread_local unsigned int atomic_unsigned_pointer_target[3] = {0xffffffffU, 74U, 75U};
unsigned int* atomic_unsigned_pointer_expected(void) { return atomic_unsigned_pointer_target; }
int atomic_tls_read(void) { return atomic_tls; }
int atomic_volatile_read(void) { return atomic_volatile; }
typedef unsigned long Dword;
typedef void* Handle;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
static Handle worker_handle;
static Dword __stdcall isolation_worker(void* data)
{
    int* result = (int*)data;
    if (atomic_tls != 3) { *result = 1; return 0; }
    atomic_tls = 71;
    *result = atomic_tls == 71 ? 0 : 2;
    return 0;
}
int atomic_tls_isolation_check(void)
{
    int result = 99;
    Handle thread = CreateThread(0, 0, isolation_worker, &result, 0, 0);
    if (!thread) return 1;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 && result == 0 ? 0 : 2;
}
typedef struct AtomicThreadContext { AtomicThreadCallback callback; int result; } AtomicThreadContext;
static Dword __stdcall native_isolation_worker(void* data)
{
    AtomicThreadContext* context = (AtomicThreadContext*)data;
    if (context->callback(0) != 3 || atomic_tls != 3) { context->result = 1; return 0; }
    if (context->callback(71) != 71 || atomic_tls != 71) { context->result = 2; return 0; }
    context->result = 0;
    return 0;
}
int atomic_tls_native_thread(AtomicThreadCallback callback)
{
    AtomicThreadContext context = {callback, 99};
    Handle thread = CreateThread(0, 0, native_isolation_worker, &context, 0, 0);
    if (!thread) return 3;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 ? context.result : 4;
}
static _Thread_local int factor_calls;
int atomic_factor(void) { ++factor_calls; return 2; }
int atomic_factor_calls(void) { return factor_calls; }
int atomic_zero(void) { return 0; }
static Dword __stdcall worker(void* data)
{
    _Atomic(int)* shared = (_Atomic(int)*)data;
    for (int i = 0; i < 10000; ++i) *shared += 1;
    return 0;
}
int atomic_start_worker(void)
{
    worker_handle = CreateThread(0, 0, worker, &atomic_tls, 0, 0);
    return worker_handle ? 0 : 1;
}
static Dword __stdcall mul_worker(void* data)
{
    _Atomic(int)* shared = (_Atomic(int)*)data;
    for (int i = 0; i < 10001; ++i) *shared *= -1;
    return 0;
}
int atomic_start_mul_worker(void)
{
    worker_handle = CreateThread(0, 0, mul_worker, &atomic_tls, 0, 0);
    return worker_handle ? 0 : 1;
}
static Dword __stdcall narrow_worker(void* data)
{
    _Atomic(short)* shared = (_Atomic(short)*)data;
    for (int i = 0; i < 10000; ++i) *shared += 1;
    return 0;
}
int atomic_start_narrow_worker(void)
{
    worker_handle = CreateThread(0, 0, narrow_worker, &atomic_unsupported_short, 0, 0);
    return worker_handle ? 0 : 1;
}
int atomic_wait_worker(void)
{
    if (!worker_handle) return 1;
    Dword result = WaitForSingleObject(worker_handle, 0xffffffffUL);
    CloseHandle(worker_handle);
    worker_handle = 0;
    return result == 0 ? 0 : 2;
}
