typedef int (*ReadTls)(void);
typedef unsigned short WideChar;
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void*, WideChar*, unsigned long);
__declspec(dllimport) void* __stdcall LoadLibraryExW(const WideChar*, void*, unsigned long);
__declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);

static void* loadSibling(const char* suffix)
{
    WideChar path[1024];
    unsigned long length = GetModuleFileNameW(0, path, 1024);
    if (!length || length >= 1024) return 0;
    while (length && path[length - 1] != '\\' && path[length - 1] != '/') --length;
    if (!length) return 0;
    while (*suffix && length + 1 < 1024) path[length++] = (unsigned char)*suffix++;
    if (*suffix) return 0;
    path[length] = 0;
    return LoadLibraryExW(path, 0, 8);
}

void mainCRTStartup(void)
{
    void* first = loadSibling("alias-a.dll");
    void* second = loadSibling("alias-b.dll");
    if (!first || !second || first == second) ExitProcess(1);
    ReadTls readFirst = (ReadTls)GetProcAddress(first, "tls_read");
    ReadTls readSecond = (ReadTls)GetProcAddress(second, "tls_read2");
    if (!readFirst || !readSecond) ExitProcess(2);
    if (readFirst() != 3 || readSecond() != 7) ExitProcess(3);
    ExitProcess(0);
}
