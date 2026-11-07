typedef int (*ReadFn)(void);
__declspec(dllimport) void* __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void)
{
    void* module = LoadLibraryA("api.dll");
    if (!module) ExitProcess(2);
    ReadFn read = (ReadFn)GetProcAddress(module, "tls_zero_read");
    if (!read) ExitProcess(3);
    ExitProcess(read() == 0 ? 0 : 1);
}
