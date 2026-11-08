__declspec(thread) int tls_exported = 7;
__declspec(dllexport) int tls_read(void) { return tls_exported; }
#ifdef TEST_UNUSED_IMPORT
__declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
__declspec(dllexport) unsigned long reference_import(void) { return GetCurrentThreadId(); }
#endif
#ifdef TEST_DLL_ENTRY
__declspec(dllexport) unsigned long dll_attach_count = 0;
int __stdcall DllMain(void *module, unsigned long reason, void *reserved)
{
    (void)module; (void)reserved;
    if (reason == 1) ++dll_attach_count;
    return 1;
}
#endif
