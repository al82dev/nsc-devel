typedef int (*ReadTls)(void);
typedef unsigned short WideChar;
__declspec(dllimport) int tls_read(void);
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void*, WideChar*, unsigned long);
__declspec(dllimport) void* __stdcall LoadLibraryExW(const WideChar*, void*, unsigned long);
__declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);

void mainCRTStartup(void)
{
    WideChar path[1024];
    const char* suffix = "alias.dll";
    unsigned long length = GetModuleFileNameW(0, path, 1024);
    if (!length || length >= 1024 || tls_read() != 3) ExitProcess(1);
    while (length && path[length - 1] != '\\' && path[length - 1] != '/') --length;
    if (!length) ExitProcess(2);
    while (*suffix && length + 1 < 1024) path[length++] = (unsigned char)*suffix++;
    if (*suffix) ExitProcess(3);
    path[length] = 0;
    void* module = LoadLibraryExW(path, 0, 8);
    if (!module) ExitProcess(4);
    ReadTls other = (ReadTls)GetProcAddress(module, "tls_read");
    if (!other || other() != 3) ExitProcess(5);
    ExitProcess(other == tls_read ? 0 : 7);
}
