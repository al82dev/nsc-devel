__declspec(dllimport) int foo(void);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void entry(void) { ExitProcess((unsigned int)foo()); }
