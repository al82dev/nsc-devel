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
__declspec(thread) int tls_array[2] = {3, 7};
__declspec(thread) int tls_three[3] = {1, 2, 3};
__declspec(thread) int tls_matrix[2][3] = {{10, 11, 12}, {20, 21, 22}};
__declspec(thread) unsigned int tls_unsigned_array[2] = {4, 5};
__declspec(thread) double tls_double_array[2] = {1.5, 2.5};
__declspec(thread) _Bool tls_bool_array[2] = {0, 1};
__declspec(thread) const int tls_const_array[2] = {6, 9};
__declspec(thread) volatile int tls_volatile_array[2] = {8, 10};
API int array_check(void) { return tls_array[0] == 31 && tls_array[1] == 71 && tls_three[2] == 33 && tls_matrix[1][2] == 42 && tls_unsigned_array[1] == 55 && tls_double_array[0] == 3.5 && tls_bool_array[0] && tls_const_array[1] == 9 && tls_volatile_array[1] == 100 ? 0 : 1; }
static Dword __stdcall worker(void* context)
{
    int* result = (int*)context;
    if (tls_array[0] != 3 || tls_array[1] != 7 || tls_three[2] != 3 || tls_matrix[1][2] != 22 || tls_unsigned_array[1] != 5 || tls_double_array[0] != 1.5 || tls_bool_array[0] || tls_const_array[1] != 9 || tls_volatile_array[1] != 10) { *result = 1; return 0; }
    tls_array[0] = 88; tls_array[1] = 99;
    *result = tls_array[0] == 88 && tls_array[1] == 99 ? 0 : 2;
    return 0;
}
API int array_other_thread(void)
{
    int result = 99;
    Handle thread = CreateThread(0, 0, worker, &result, 0, 0);
    if (!thread) return 3;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 ? result : 4;
}
