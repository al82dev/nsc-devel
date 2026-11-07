__declspec(thread) int tls_zero;
__declspec(thread) volatile int tls_volatile = 4;
__declspec(thread) const int tls_const = 19;
__declspec(thread) unsigned int tls_unsigned = 7;
__declspec(thread) double tls_double = 1.5;
__declspec(thread) _Bool tls_bool;
#ifdef TLS_PROBE_IMPORT
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllexport) unsigned long tls_test_thread_id(void) { return GetCurrentThreadId(); }
#endif
#ifdef TLS_PROBE_ENTRY
int __stdcall DllMain(void* module, unsigned long reason, void* reserved)
{
    (void)module; (void)reason; (void)reserved;
    return 1;
}
#endif
__declspec(dllexport) int tls_zero_read(void) { return tls_zero; }
__declspec(dllexport) int tls_scalar_check(void)
{
    return tls_zero == 31 && tls_volatile == 14 && tls_const == 19 &&
           tls_unsigned == 17 && tls_double == 2.5 && tls_bool ? 0 : 1;
}
