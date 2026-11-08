typedef int (*TlsThreadCallback)(int value);
int run_tls_thread(TlsThreadCallback callback);
