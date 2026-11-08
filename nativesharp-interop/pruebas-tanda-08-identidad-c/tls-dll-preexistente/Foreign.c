__declspec(thread) int tls_exported = 99;
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllexport) int foreign_read(void) { return GetCurrentThreadId() ? tls_exported : -1; }
