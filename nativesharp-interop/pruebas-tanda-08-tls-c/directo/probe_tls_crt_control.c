typedef int (*ReadFn)(void);
__declspec(dllimport) void* __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
int main(void)
{
    void* module = LoadLibraryA("api.dll");
    if (!module) return 2;
    ReadFn read = (ReadFn)GetProcAddress(module, "tls_zero_read");
    if (!read) return 3;
    return read() == 0 ? 0 : 8;
}
