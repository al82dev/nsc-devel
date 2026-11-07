typedef unsigned int U32;
typedef unsigned long long U64;
typedef unsigned char Byte;
__declspec(dllimport) void *__stdcall GetModuleHandleA(const char *);
__declspec(dllimport) void *__stdcall GetStdHandle(U32);
__declspec(dllimport) int __stdcall WriteFile(void *, const void *, U32, U32 *, void *);
__declspec(dllimport) int tls_zero_read(void);
__declspec(dllimport) void __stdcall ExitProcess(U32);
U64 __readgsqword(unsigned long);
static void hex(const char *name, U64 value)
{
    char buf[80]; U32 n = 0, count;
    while (*name) buf[n++] = *name++;
    buf[n++] = '=';
    for (int i = 15; i >= 0; --i) { U32 x = (U32)((value >> (i * 4)) & 15); buf[n++] = (char)(x < 10 ? '0' + x : 'a' + x - 10); }
    buf[n++] = '\r'; buf[n++] = '\n';
    WriteFile(GetStdHandle((U32)-11), buf, n, &count, 0);
}
void mainCRTStartup(void)
{
    Byte *image = (Byte *)GetModuleHandleA("api.dll");
    Byte *nt = image + *(U32 *)(image + 0x3c);
    Byte *dir = image + *(U32 *)(nt + 208);
    Byte *start = *(Byte **)dir;
    Byte *end = *(Byte **)(dir + 8);
    U32 index = **(U32 **)(dir + 16);
    Byte **slots = (Byte **)__readgsqword(0x58);
    hex("image", (U64)image); hex("start", (U64)start); hex("end", (U64)end);
    hex("index", index); hex("slots", (U64)slots); hex("slot", (U64)slots[index]);
    hex("template+20", *(U32 *)(start + 0x20)); hex("thread+20", *(U32 *)(slots[index] + 0x20));
    hex("template+08", *(U32 *)(start + 8)); hex("thread+08", *(U32 *)(slots[index] + 8));
    hex("C read", (U32)tls_zero_read());
    ExitProcess(0);
}
