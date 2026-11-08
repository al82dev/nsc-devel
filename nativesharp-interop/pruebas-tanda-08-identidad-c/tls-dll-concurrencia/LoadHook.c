typedef unsigned long Dword;
typedef unsigned long long U64;
typedef unsigned short WideChar;
typedef void* Handle;
typedef Handle (__stdcall *LoadFunction)(const WideChar*, Handle, Dword);
__declspec(dllimport) Handle __stdcall CreateEventA(void*, int, int, const char*);
__declspec(dllimport) int __stdcall SetEvent(Handle);
__declspec(dllimport) Dword __stdcall WaitForSingleObject(Handle, Dword);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
__declspec(dllimport) Handle __stdcall GetModuleHandleA(const char*);
__declspec(dllimport) int __stdcall VirtualProtect(void*, U64, Dword, Dword*);

static Handle bothEntered;
static volatile long entered;
static LoadFunction original;
static void** importSlot;

static int same(const char* a, const char* b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static int target(const WideChar* path)
{
    const char* suffix = "api2.dll";
    U64 length = 0;
    while (path[length]) ++length;
    if (length < 8) return 0;
    path += length - 8;
    for (int i = 0; i < 8; ++i) if (path[i] != (unsigned char)suffix[i]) return 0;
    return 1;
}

static Handle __stdcall observedLoad(const WideChar* name, Handle file, Dword flags)
{
    if (target(name))
    {
        long count = __atomic_add_fetch(&entered, 1, __ATOMIC_SEQ_CST);
        if (count == 2) SetEvent(bothEntered);
        if (WaitForSingleObject(bothEntered, 10000) != 0) return 0;
    }
    return original(name, file, flags);
}

int tls_install_load_observer(void)
{
    unsigned char* image = (unsigned char*)GetModuleHandleA(0);
    if (!image) return 1;
    unsigned char* nt = image + *(unsigned int*)(image + 60);
    unsigned int imports = *(unsigned int*)(nt + 144);
    if (!imports) return 2;
    for (unsigned char* desc = image + imports; *(unsigned int*)(desc + 12); desc += 20)
    {
        unsigned int lookupRva = *(unsigned int*)desc;
        unsigned int addressRva = *(unsigned int*)(desc + 16);
        if (!lookupRva || !addressRva) continue;
        U64* lookup = (U64*)(image + lookupRva);
        void** addresses = (void**)(image + addressRva);
        for (U64 i = 0; lookup[i]; ++i)
        {
            if (lookup[i] >> 63) continue;
            const char* name = (const char*)(image + (unsigned int)lookup[i] + 2);
            if (!same(name, "LoadLibraryExW")) continue;
            bothEntered = CreateEventA(0, 1, 0, 0);
            if (!bothEntered) return 3;
            entered = 0;
            importSlot = &addresses[i];
            original = (LoadFunction)*importSlot;
            Dword oldProtection, ignored;
            if (!VirtualProtect(importSlot, sizeof(*importSlot), 4, &oldProtection)) return 4;
            *importSlot = (void*)observedLoad;
            if (!VirtualProtect(importSlot, sizeof(*importSlot), oldProtection, &ignored)) return 5;
            return 0;
        }
    }
    return 6;
}

int tls_finish_load_observer(void)
{
    Dword oldProtection, ignored;
    if (!importSlot || !VirtualProtect(importSlot, sizeof(*importSlot), 4, &oldProtection)) return 1;
    *importSlot = (void*)original;
    if (!VirtualProtect(importSlot, sizeof(*importSlot), oldProtection, &ignored)) return 2;
    CloseHandle(bothEntered);
    return entered == 2 ? 0 : 3;
}
