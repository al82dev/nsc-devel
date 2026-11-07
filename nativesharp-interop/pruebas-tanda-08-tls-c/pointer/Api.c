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
API int* pointer_target(void) { return &first; }
API double* pointer_double_target(void) { return &wide; }
API void pointer_prepare(void) { tls_readonly_pointer = &first; tls_volatile_pointer = &first; }
API int pointer_check(void) { return tls_pointer == &first && first == 41 && tls_double_pointer == &wide && wide == 2.5 && tls_readonly_pointer == &first && tls_volatile_pointer == &first && tls_volatile_slot == &first && tls_const_slot == &first ? 0 : 1; }
static Dword __stdcall worker(void* context)
{
    int* result = (int*)context;
    if (tls_pointer != 0 || tls_double_pointer != 0 || tls_readonly_pointer != 0 || tls_volatile_pointer != 0 || tls_volatile_slot != 0 || tls_const_slot != &first) { *result = 1; return 0; }
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
