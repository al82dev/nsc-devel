#include "Types.h"
typedef unsigned long Dword;
typedef void* Handle;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
#ifdef BUILD_DLL
#define API __declspec(dllexport)
#else
#define API
#endif
__declspec(thread) int* tls_pointer;
static int first = 37;
static int second = 98;
static double wide = 1.5;
__declspec(thread) double* tls_double_pointer;
__declspec(thread) const int* tls_readonly_pointer;
__declspec(thread) volatile int* tls_volatile_pointer;
__declspec(thread) int* volatile tls_volatile_slot;
__declspec(thread) int* const tls_const_slot = &first;
__declspec(thread) TlsRecord tls_record = {3, 4};
__declspec(thread) TlsRecord* tls_record_pointer;
__declspec(thread) TlsRecord tls_records[2] = {{5, 6}, {7, 8}};
__declspec(thread) TlsMode tls_mode = TLS_START;
__declspec(thread) TlsMode* tls_mode_pointer;
__declspec(thread) TlsCallback tls_callback;
static int add_four(int value) { return value + 4; }
API TlsCallback pointer_callback(void) { return add_four; }
API int* pointer_target(void) { return &first; }
API double* pointer_double_target(void) { return &wide; }
API void pointer_prepare(void) { tls_readonly_pointer = &first; tls_volatile_pointer = &first; tls_record_pointer = &tls_record; tls_mode_pointer = &tls_mode; }
API int pointer_check(void) { return tls_pointer == &first && first == 41 && tls_double_pointer == &wide && wide == 2.5 && tls_readonly_pointer == &first && tls_volatile_pointer == &first && tls_volatile_slot == &first && tls_const_slot == &first && tls_record.value == 31 && tls_record.extra == 32 && tls_record_pointer == &tls_record && tls_records[1].value == 71 && tls_mode == TLS_SET && tls_mode_pointer == &tls_mode && tls_callback && tls_callback(38) == 42 ? 0 : 1; }
static Dword __stdcall worker(void* context)
{
    int* result = (int*)context;
    if (tls_pointer != 0 || tls_double_pointer != 0 || tls_readonly_pointer != 0 || tls_volatile_pointer != 0 || tls_volatile_slot != 0 || tls_const_slot != &first || tls_record.value != 3 || tls_record.extra != 4 || tls_record_pointer != 0 || tls_records[1].value != 7 || tls_mode != TLS_START || tls_mode_pointer != 0 || tls_callback != 0) { *result = 1; return 0; }
    tls_pointer = &second;
    *result = *tls_pointer == 98 ? 0 : 2;
    return 0;
}
API int pointer_other_thread(void)
{
    int result = 99;
    Handle thread = CreateThread(0, 0, worker, &result, 0, 0);
    if (!thread) return 3;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 ? result : 4;
}
