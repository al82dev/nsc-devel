typedef int (*TlsThreadCallback)(int value);
__declspec(dllimport) int run_tls_thread(TlsThreadCallback callback);
