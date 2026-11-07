/* Values distinguish integer promotion from truncating the RHS to storage width. */
_Thread_local _Atomic(short) narrow_short;
_Thread_local _Atomic(signed char) narrow_char;
_Thread_local _Atomic(unsigned short) narrow_ushort;
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void entry(void)
{
    int result = 0;
    narrow_short = 30000;
    if ((narrow_short /= 65536) != 0) result = 1;
    narrow_short = 30000;
    if ((narrow_short %= 65536) != 30000) result = 2;
    narrow_char = -100;
    if ((narrow_char /= 256) != 0) result = 3;
    narrow_ushort = 60000;
    if ((narrow_ushort /= 65536) != 0) result = 4;
    narrow_ushort = 60000;
    if ((narrow_ushort %= 65536) != 60000) result = 5;
    ExitProcess((unsigned int)result);
}
