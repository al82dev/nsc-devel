typedef void* Handle;
typedef unsigned long Dword;
typedef Dword (__stdcall *ThreadRoutine)(void*);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
static _Thread_local int local_value = 7;
static int worker_result;
static Dword __stdcall worker(void* unused) {
    (void)unused;
    if (local_value != 7) return 1;
    local_value = 40;
    worker_result = local_value == 40;
    return 0;
}
static inline int read_local(void) { return local_value; }
static inline void write_local(int value) { local_value = value; }
static inline int run_worker(void) {
    worker_result = 0;
    Handle handle = CreateThread(0, 0, worker, 0, 0, 0);
    if (!handle) return 1;
    Dword result = WaitForSingleObject(handle, 0xffffffffUL);
    CloseHandle(handle);
    return result == 0 && worker_result ? 0 : 2;
}
