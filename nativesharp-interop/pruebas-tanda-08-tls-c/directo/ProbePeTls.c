/* Experimento Windows x64: inspeccion del TLS estatico de una DLL cargada.
   No es una interfaz publica de NSC ni sustituye el consumidor C directo. */
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned long long U64;
typedef unsigned char Byte;
typedef void *Handle;
typedef U32 (__stdcall *ThreadRoutine)(void *);
__declspec(dllimport) Handle __stdcall GetModuleHandleA(const char *);
__declspec(dllimport) void * __stdcall GetProcAddress(Handle, const char *);
__declspec(dllimport) Handle __stdcall CreateThread(void *, U64, ThreadRoutine, void *, U32, U32 *);
__declspec(dllimport) U32 __stdcall WaitForSingleObject(Handle, U32);
__declspec(dllimport) int __stdcall CloseHandle(Handle);
__declspec(dllimport) void __stdcall ExitProcess(U32);
__declspec(dllimport) int tls_read(void);
unsigned long long __readgsqword(unsigned long);

static int *resolve(void)
{
    Byte *module = (Byte *)GetModuleHandleA("api.dll");
    if (!module || *(U16 *)module != 0x5a4d) return 0;
    Byte *nt = module + *(U32 *)(module + 0x3c);
    if (*(U32 *)nt != 0x4550 || *(U16 *)(nt + 24) != 0x20b) return 0;
    U32 tls_rva = *(U32 *)(nt + 24 + 112 + 9 * 8);
    if (!tls_rva) return 0;
    Byte *directory = module + tls_rva;
    Byte *raw_start = *(Byte **)directory;
    Byte *raw_end = *(Byte **)(directory + 8);
    U32 *index = *(U32 **)(directory + 16);
    Byte *exported = (Byte *)GetProcAddress(module, "tls_exported");
    if (!index || !exported || exported < raw_start || exported + sizeof(int) > raw_end) return 0;
    Byte **slots = (Byte **)__readgsqword(0x58);
    if (!slots || !slots[*index]) return 0;
    return (int *)(slots[*index] + (exported - raw_start));
}

static U32 __stdcall worker(void *result)
{
    int *slot = resolve();
    if (!slot || *slot != 3 || tls_read() != 3) { *(int *)result = 1; return 0; }
    *slot = 73;
    *(int *)result = tls_read() == 73 ? 0 : 2;
    return 0;
}

void mainCRTStartup(void)
{
    int result = 3;
    int *slot = resolve();
    if (!slot || *slot != 3 || tls_read() != 3) ExitProcess(1);
    *slot = 41;
    if (tls_read() != 41) ExitProcess(2);
    Handle thread = CreateThread(0, 0, worker, &result, 0, 0);
    if (!thread) ExitProcess(3);
    U32 wait = WaitForSingleObject(thread, 0xffffffffU);
    CloseHandle(thread);
    if (wait != 0 || result != 0 || *slot != 41 || tls_read() != 41) ExitProcess(4);
    ExitProcess(0);
}
