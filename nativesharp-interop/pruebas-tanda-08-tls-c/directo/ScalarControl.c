__declspec(dllimport) int tls_zero_read(void);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);
void mainCRTStartup(void) { ExitProcess(tls_zero_read() == 0 ? 0 : 1); }
