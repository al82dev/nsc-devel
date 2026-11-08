typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned long long U64;
typedef unsigned char Byte;
typedef void *Handle;
__declspec(dllimport) Handle __stdcall GetModuleHandleA(const char *);
__declspec(dllimport) void * __stdcall GetProcAddress(Handle, const char *);
__declspec(dllimport) Handle __stdcall GetStdHandle(U32);
__declspec(dllimport) int __stdcall WriteFile(Handle, const void *, U32, U32 *, void *);
__declspec(dllimport) void __stdcall ExitProcess(U32);
__declspec(dllimport) int tls_read(void);
U64 __readgsqword(unsigned long);

static void number(const char *label, U32 value)
{
    char buffer[64];
    U32 length = 0, written = 0;
    while (*label) buffer[length++] = *label++;
    for (int shift = 28; shift >= 0; shift -= 4)
        buffer[length++] = "0123456789abcdef"[(value >> shift) & 15];
    buffer[length++] = '\n';
    WriteFile(GetStdHandle((U32)-11), buffer, length, &written, 0);
}

void mainCRTStartup(void)
{
    Byte *module = (Byte *)GetModuleHandleA("api.dll");
    if (!module) ExitProcess(1);
    Byte *nt = module + *(U32 *)(module + 0x3c);
    if (*(U32 *)nt != 0x4550 || *(U16 *)(nt + 24) != 0x20b) ExitProcess(2);
    U32 tls = *(U32 *)(nt + 24 + 112 + 9 * 8);
    if (!tls) ExitProcess(3);
    Byte *directory = module + tls;
    Byte *raw = *(Byte **)directory;
    U32 *index = *(U32 **)(directory + 16);
    Byte **slots = (Byte **)__readgsqword(0x58);
    if (!raw || !index || !slots || !slots[*index]) ExitProcess(4);
    number("index=", *index);
    number("raw0=", *(U32 *)raw);
    number("raw4=", *(U32 *)(raw + 4));
    number("slot0=", *(U32 *)slots[*index]);
    number("slot4=", *(U32 *)(slots[*index] + 4));
    number("read=", (U32)tls_read());
    U32 *attach = (U32 *)GetProcAddress(module, "dll_attach_count");
    if (attach) number("attach=", *attach);
    ExitProcess(0);
}
