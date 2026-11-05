__declspec(dllimport) int __nsc_method_CppRuntimePackagingDll$3A$3ABridge__Get(void);
__declspec(dllimport) void __stdcall ExitProcess(unsigned int);

void mainCRTStartup(void)
{
    ExitProcess((unsigned int)__nsc_method_CppRuntimePackagingDll$3A$3ABridge__Get());
}
