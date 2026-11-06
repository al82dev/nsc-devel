__declspec(dllimport) extern __declspec(thread) int tls_exported;
__declspec(dllimport) int tls_read(void);
__declspec(dllimport) int tls_other_thread(void);
