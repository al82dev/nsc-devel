__declspec(thread) int tls_exported = 7;
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllimport) int tls_leaf_bias(void);
__declspec(dllexport) int tls_read2(void)
{
    return GetCurrentThreadId() != 0 ? tls_exported + tls_leaf_bias() - 7 : -1;
}
