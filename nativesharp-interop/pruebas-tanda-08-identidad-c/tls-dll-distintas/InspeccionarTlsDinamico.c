typedef unsigned int U32;
typedef unsigned short U16;
typedef void *Handle;
typedef int (*ReadTls)(void);
__declspec(dllimport) U32 __stdcall GetModuleFileNameW(Handle, U16 *, U32);
__declspec(dllimport) Handle __stdcall LoadLibraryExW(const U16 *, Handle, U32);
__declspec(dllimport) void * __stdcall GetProcAddress(Handle, const char *);
__declspec(dllimport) void __stdcall ExitProcess(U32);
void mainCRTStartup(void)
{
    U16 path[1024];
    const char *name = "api.dll";
    U32 length = GetModuleFileNameW(0, path, 1024);
    if (!length || length >= 1024) ExitProcess(101);
    while (length && path[length - 1] != '\\' && path[length - 1] != '/') --length;
    if (!length) ExitProcess(102);
    while (*name && length + 1 < 1024) path[length++] = (unsigned char)*name++;
    if (*name) ExitProcess(103);
    path[length] = 0;
    Handle module = LoadLibraryExW(path, 0, 8);
    if (!module) ExitProcess(104);
    ReadTls read = (ReadTls)GetProcAddress(module, "tls_read");
    if (!read) ExitProcess(105);
    ExitProcess((U32)read() == 7 ? 0 : 106);
}
