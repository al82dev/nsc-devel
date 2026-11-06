__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
extern int ns_main(void) __asm__("__nsc_function_CTlsPrerequisite$3A$3A__Main");
void entry(void) { ExitProcess((unsigned int)ns_main()); }
