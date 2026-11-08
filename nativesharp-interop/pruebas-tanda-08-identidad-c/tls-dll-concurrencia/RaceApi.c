typedef unsigned long Dword;
typedef void* Handle;
typedef Dword (__stdcall *ThreadRoutine)(void*);
typedef int (*TlsRaceCallback)(int);
__declspec(dllimport) Handle __stdcall CreateEventA(void*, int, int, const char*);
__declspec(dllimport) int __stdcall SetEvent(Handle);
__declspec(dllimport) Handle __stdcall CreateThread(void*, unsigned long long, ThreadRoutine, void*, Dword, Dword*);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
__declspec(dllimport) void* __stdcall GetModuleHandleA(const char*);

typedef struct Context { TlsRaceCallback callback; Handle ready; Handle start; int id; int result; } Context;
static Dword __stdcall worker(void* opaque)
{
    Context* context = (Context*)opaque;
    if (!SetEvent(context->ready) || WaitForSingleObject(context->start, 10000) != 0) {
        context->result = 90; return 0;
    }
    context->result = context->callback(context->id);
    return 0;
}

__declspec(dllexport) int tls_race_first_access(TlsRaceCallback callback)
{
    if (GetModuleHandleA("api2.dll") != 0) return 1;
    Handle start = CreateEventA(0, 1, 0, 0);
    Handle ready1 = CreateEventA(0, 1, 0, 0);
    Handle ready2 = CreateEventA(0, 1, 0, 0);
    if (!start || !ready1 || !ready2) return 2;
    Context first = {callback, ready1, start, 1, 99};
    Context second = {callback, ready2, start, 2, 99};
    Handle thread1 = CreateThread(0, 0, worker, &first, 0, 0);
    Handle thread2 = CreateThread(0, 0, worker, &second, 0, 0);
    if (!thread1 || !thread2) return 3;
    int result = 0;
    if (WaitForSingleObject(ready1, 10000) != 0 || WaitForSingleObject(ready2, 10000) != 0) result = 4;
    if (GetModuleHandleA("api2.dll") != 0) result = 5;
    if (!SetEvent(start)) result = 6;
    if (WaitForSingleObject(thread1, 10000) != 0 || WaitForSingleObject(thread2, 10000) != 0) result = 7;
    if (first.result != 0 || second.result != 0) result = 8;
    CloseHandle(thread1); CloseHandle(thread2);
    CloseHandle(ready1); CloseHandle(ready2); CloseHandle(start);
    return result;
}
