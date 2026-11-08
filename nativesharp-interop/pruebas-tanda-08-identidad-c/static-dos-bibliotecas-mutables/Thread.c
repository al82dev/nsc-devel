typedef unsigned long Dword;
typedef void *Handle;
typedef Dword (__stdcall *ThreadRoutine)(void *);
typedef int (*TlsThreadCallback)(int);
__declspec(dllimport) Handle __stdcall CreateThread(void *, unsigned long long, ThreadRoutine, void *, Dword, Dword *);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
typedef struct Context { TlsThreadCallback callback; int result; } Context;
static Dword __stdcall worker(void *opaque) {
    Context *context = (Context *)opaque;
    context->result = context->callback(0);
    return 0;
}
int run_tls_thread(TlsThreadCallback callback) {
    Context context = {callback, 99};
    Handle thread = CreateThread(0, 0, worker, &context, 0, 0);
    if (!thread) return 3;
    Dword wait = WaitForSingleObject(thread, 0xffffffffUL);
    CloseHandle(thread);
    return wait == 0 ? context.result : 4;
}
