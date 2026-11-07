/* Instrumento para verificar el cruce NSC -> direccion TLS de la DLL.
   No es la implementacion del compilador ni sustituye acceso directo. */
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char Byte;
typedef void *Handle;
__declspec(dllimport) int __stdcall GetModuleHandleExA(U32, const char *, Handle *);
__declspec(dllimport) int tls_exported;
unsigned long long __readgsqword(unsigned long);

int *tls_bridge(void)
{
    Handle image = 0;
    if (!GetModuleHandleExA(6, (const char *)&tls_exported, &image)) return 0;
    Byte *module = (Byte *)image;
    if (*(U16 *)module != 0x5a4d) return 0;
    Byte *nt = module + *(U32 *)(module + 0x3c);
    if (*(U32 *)nt != 0x4550 || *(U16 *)(nt + 24) != 0x20b) return 0;
    U32 tls_rva = *(U32 *)(nt + 24 + 112 + 9 * 8);
    if (!tls_rva) return 0;
    Byte *directory = module + tls_rva;
    Byte *raw_start = *(Byte **)directory;
    Byte *raw_end = *(Byte **)(directory + 8);
    U32 *index = *(U32 **)(directory + 16);
    Byte *exported = (Byte *)&tls_exported;
    if (!index || exported < raw_start || exported + sizeof(int) > raw_end) return 0;
    Byte **slots = (Byte **)__readgsqword(0x58);
    if (!slots || !slots[*index]) return 0;
    return (int *)(slots[*index] + (exported - raw_start));
}
