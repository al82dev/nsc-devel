typedef int (*ReadTls)(void);
__declspec(dllimport) void* __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);

void mainCRTStartup(void)
{
    void* first = LoadLibraryA("uno\\api.dll");
    void* second = LoadLibraryA("dos\\api.dll");
    if (!first || !second) ExitProcess(1);
    if (first == second) ExitProcess(2);
    ReadTls readFirst = (ReadTls)GetProcAddress(first, "tls_read");
    ReadTls readSecond = (ReadTls)GetProcAddress(second, "tls_read2");
    if (!readFirst || !readSecond) ExitProcess(3);
    if (readFirst() != 3 || readSecond() != 7) ExitProcess(4);
    ExitProcess(0);
}
