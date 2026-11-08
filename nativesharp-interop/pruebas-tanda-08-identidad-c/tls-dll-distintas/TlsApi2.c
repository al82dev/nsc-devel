__declspec(thread) int tls_exported = 7;
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllexport) int tls_read2(void)
{
    return GetCurrentThreadId() != 0 ? tls_exported : -1;
}
