typedef int (*TlsThreadCallback)(int value);
__declspec(dllimport) int tls_call_native_thread(TlsThreadCallback callback);
