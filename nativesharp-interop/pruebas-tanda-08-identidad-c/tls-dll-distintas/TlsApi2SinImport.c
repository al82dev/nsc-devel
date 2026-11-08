__declspec(thread) int tls_exported = 7;
__declspec(dllexport) int tls_read(void) { return tls_exported; }
#ifdef TEST_DLL_ENTRY
int __stdcall DllMain(void *module, unsigned long reason, void *reserved)
{
    (void)module; (void)reason; (void)reserved;
    return 1;
}
#endif
