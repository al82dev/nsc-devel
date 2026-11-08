__declspec(thread) int tls_exported = 7;
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllexport) int tls_read(void) { return GetCurrentThreadId() ? tls_exported : -1; }
