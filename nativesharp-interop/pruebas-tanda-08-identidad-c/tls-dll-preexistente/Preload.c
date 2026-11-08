typedef unsigned short WideChar;
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void*, WideChar*, unsigned long);
__declspec(dllimport) void* __stdcall LoadLibraryExW(const WideChar*, void*, unsigned long);
__declspec(dllexport) int preload_foreign(void)
{
    WideChar path[1024];
    const char* suffix = "foreign\\api2.dll";
    unsigned long length = GetModuleFileNameW(0, path, 1024);
    if (!length || length >= 1024) return 1;
    while (length && path[length - 1] != '\\' && path[length - 1] != '/') --length;
    if (!length) return 2;
    while (*suffix && length + 1 < 1024) path[length++] = (unsigned char)*suffix++;
    if (*suffix) return 3;
    path[length] = 0;
    return LoadLibraryExW(path, 0, 8) ? 0 : 4;
}
