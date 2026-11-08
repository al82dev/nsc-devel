__declspec(thread) int tls_exported = 7;
__declspec(dllexport) int tls_read(void) { return tls_exported; }
